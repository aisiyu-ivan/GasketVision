#include "CameraIpcPublisher.h"

#include "CameraIpcLayout.h"
#include "CameraIpcSync.h"
#include "IVisionImageSource.h"

#include <QDateTime>
#include <QSharedMemory>
#include <QThread>

#include <opencv2/core.hpp>

#include <cstring>

namespace
{
void resetInspectMetadata(CameraIpc::ControlBlock *ctrl)
{
    ctrl->stationId = 0;
    ctrl->ok = 0;
    ctrl->defectCode = 0;
    ctrl->flags = CameraIpc::kFlagHasOriginal;
    ctrl->reserved = 0;
    ctrl->outerDiameterMm = 0.0;
    ctrl->innerDiameterMm = 0.0;
    ctrl->offsetXMm = 0.0;
    ctrl->offsetYMm = 0.0;
    ctrl->matchScore = 0.0;
    std::memset(ctrl->defectText, 0, sizeof(ctrl->defectText));
    std::memset(ctrl->annotatedPath, 0, sizeof(ctrl->annotatedPath));
}
} // namespace

// 构造相机 IPC 发布器
CameraIpcPublisher::CameraIpcPublisher()
    : m_sync(new CameraIpcSync())
{
}

// 释放 SHM 与同步对象
CameraIpcPublisher::~CameraIpcPublisher()
{
    if (m_shm)
    {
        if (m_shm->isAttached())
            m_shm->detach();
        delete m_shm;
        m_shm = nullptr;
    }
    delete m_sync;
    m_sync = nullptr;
}

// 创建相机 SHM 区域与同步对象
bool CameraIpcPublisher::initialize()
{
    if (m_ready)
        return true;

    if (!m_sync->create())
        return false;

    m_shm = new QSharedMemory(CameraIpc::shmKey());
    const qsizetype bytes = CameraIpc::regionBytes();
    if (!m_shm->create(static_cast<int>(bytes)))
    {
        if (m_shm->error() == QSharedMemory::AlreadyExists)
        {
            if (m_shm->attach())
                m_shm->detach();
            if (!m_shm->create(static_cast<int>(bytes)))
                return false;
        }
        else
        {
            return false;
        }
    }

    std::memset(m_shm->data(), 0, static_cast<size_t>(bytes));
    auto *ctrl = CameraIpc::controlBlock(m_shm->data());
    ctrl->magic = CameraIpc::kMagic;
    ctrl->version = CameraIpc::kVersion;

    m_ready = true;
    return true;
}

// 将像素写入指定环槽并填充图像头
bool CameraIpcPublisher::writePlane(void *shm, quint32 slotIndex, quint64 frameId, const VisionFrame &frame)
{
    if (frame.image.empty())
        return false;

    cv::Mat continuous = frame.image.isContinuous() ? frame.image : frame.image.clone();
    quint8 format = CameraIpc::Gray8;
    if (continuous.channels() == 3)
        format = CameraIpc::Bgr888;
    else if (continuous.channels() != 1)
        return false;

    const quint32 payloadSize =
        static_cast<quint32>(static_cast<qsizetype>(continuous.step) * continuous.rows);
    if (payloadSize == 0 || payloadSize > static_cast<quint32>(CameraIpc::kMaxImageBytes))
        return false;

    auto *header = CameraIpc::planeHeader(shm, slotIndex);
    uchar *payload = CameraIpc::planePayload(shm, slotIndex);
    header->magic = CameraIpc::kMagic;
    header->frameId = frameId;
    header->width = static_cast<quint32>(continuous.cols);
    header->height = static_cast<quint32>(continuous.rows);
    header->bytesPerLine = static_cast<quint32>(continuous.step);
    header->format = format;
    header->payloadSize = payloadSize;
    std::memcpy(payload, continuous.data, payloadSize);
    return true;
}

uchar *CameraIpcPublisher::slotPayload(void *shm, quint32 slotIndex)
{
    return CameraIpc::planePayload(shm, slotIndex);
}

bool CameraIpcPublisher::publishDirect(quint64 frameId, quint8 format, quint32 width, quint32 height,
                                       quint32 bytesPerLine, quint32 payloadSize, const VisionFrame &meta)
{
    if (!m_ready && !initialize())
        return false;
    if (frameId == 0 || frameId <= m_frameId)
        return false;
    if (payloadSize == 0 || payloadSize > static_cast<quint32>(CameraIpc::kMaxImageBytes))
        return false;
    if (!m_sync->lock())
        return false;

    const quint32 slotIndex = static_cast<quint32>(frameId % CameraIpc::kRingSlots);
    auto *header = CameraIpc::planeHeader(m_shm->data(), slotIndex);
    header->magic = CameraIpc::kMagic;
    header->frameId = frameId;
    header->width = width;
    header->height = height;
    header->bytesPerLine = bytesPerLine;
    header->format = format;
    header->payloadSize = payloadSize;

    auto *ctrl = CameraIpc::controlBlock(m_shm->data());
    ctrl->magic = CameraIpc::kMagic;
    ctrl->version = CameraIpc::kVersion;
    ctrl->frameId = frameId;
    ctrl->timestampMs = meta.timestampMs;
    ctrl->slotIndex = slotIndex;
    resetInspectMetadata(ctrl);

    const QByteArray pathUtf8 = meta.sourcePath.toUtf8();
    std::memset(ctrl->sourcePath, 0, sizeof(ctrl->sourcePath));
    std::memcpy(ctrl->sourcePath, pathUtf8.constData(),
                static_cast<size_t>(qMin(pathUtf8.size(), static_cast<int>(sizeof(ctrl->sourcePath) - 1))));

    const QByteArray statusUtf8 = meta.cameraStatus.toUtf8();
    std::memset(ctrl->cameraStatus, 0, sizeof(ctrl->cameraStatus));
    std::memcpy(ctrl->cameraStatus, statusUtf8.constData(),
                static_cast<size_t>(qMin(statusUtf8.size(), static_cast<int>(sizeof(ctrl->cameraStatus) - 1))));

    if (header->frameId != frameId)
    {
        m_sync->unlock();
        return false;
    }

    m_frameId = frameId;
    m_sync->unlock();
    m_sync->notifyFrameReady();
    return true;
}

bool CameraIpcPublisher::captureFromSource(quint64 frameId, IVisionImageSource &source)
{
    if (!m_ready && !initialize())
        return false;
    if (frameId == 0 || frameId <= m_frameId)
        return false;
    if (!m_sync->lock())
        return false;

    const quint32 slotIndex = static_cast<quint32>(frameId % CameraIpc::kRingSlots);
    uchar *payload = CameraIpc::planePayload(m_shm->data(), slotIndex);

    VisionFrame meta;
    meta.cameraStatus = source.statusText();
    if (!source.grabFrameInto(payload, CameraIpc::kMaxImageBytes, meta))
    {
        m_sync->unlock();
        return false;
    }

    if (meta.image.empty())
    {
        m_sync->unlock();
        return false;
    }

    quint8 format = CameraIpc::Gray8;
    if (meta.image.channels() == 3)
        format = CameraIpc::Bgr888;
    else if (meta.image.channels() != 1)
    {
        m_sync->unlock();
        return false;
    }

    const quint32 payloadSize =
        static_cast<quint32>(static_cast<qsizetype>(meta.image.step) * meta.image.rows);
    if (payloadSize == 0 || payloadSize > static_cast<quint32>(CameraIpc::kMaxImageBytes))
    {
        m_sync->unlock();
        return false;
    }

    auto *header = CameraIpc::planeHeader(m_shm->data(), slotIndex);
    header->magic = CameraIpc::kMagic;
    header->frameId = frameId;
    header->width = static_cast<quint32>(meta.image.cols);
    header->height = static_cast<quint32>(meta.image.rows);
    header->bytesPerLine = static_cast<quint32>(meta.image.step);
    header->format = format;
    header->payloadSize = payloadSize;

    auto *ctrl = CameraIpc::controlBlock(m_shm->data());
    ctrl->magic = CameraIpc::kMagic;
    ctrl->version = CameraIpc::kVersion;
    ctrl->frameId = frameId;
    ctrl->timestampMs = meta.timestampMs;
    ctrl->slotIndex = slotIndex;
    resetInspectMetadata(ctrl);

    const QByteArray pathUtf8 = meta.sourcePath.toUtf8();
    std::memset(ctrl->sourcePath, 0, sizeof(ctrl->sourcePath));
    std::memcpy(ctrl->sourcePath, pathUtf8.constData(),
                static_cast<size_t>(qMin(pathUtf8.size(), static_cast<int>(sizeof(ctrl->sourcePath) - 1))));

    const QByteArray statusUtf8 = meta.cameraStatus.toUtf8();
    std::memset(ctrl->cameraStatus, 0, sizeof(ctrl->cameraStatus));
    std::memcpy(ctrl->cameraStatus, statusUtf8.constData(),
                static_cast<size_t>(qMin(statusUtf8.size(), static_cast<int>(sizeof(ctrl->cameraStatus) - 1))));

    m_frameId = frameId;
    m_sync->unlock();
    m_sync->notifyFrameReady();
    return true;
}

bool CameraIpcPublisher::waitForAnnotated(quint64 frameId, int timeoutMs)
{
    if (!m_ready && !initialize())
        return false;
    if (frameId == 0)
        return false;

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMax(timeoutMs, 1);
    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        if (!m_sync->lock(50))
        {
            QThread::msleep(5);
            continue;
        }

        const auto *ctrl = CameraIpc::controlBlock(m_shm->data());
        const bool ready = ctrl->frameId == frameId && (ctrl->flags & CameraIpc::kFlagHasAnnotated) != 0;
        m_sync->unlock();
        if (ready)
            return true;

        QThread::msleep(5);
    }
    return false;
}

// 将一帧写入环槽并通知引擎
bool CameraIpcPublisher::publish(const VisionFrame &frame)
{
    if (!m_ready && !initialize())
        return false;
    const quint64 frameId = m_frameId + 1;
    return publish(frame, frameId);
}

// 严格模式：按指定 frameId 写入环槽
bool CameraIpcPublisher::publish(const VisionFrame &frame, quint64 frameId)
{
    if (!m_ready && !initialize())
        return false;
    if (frameId == 0 || frameId <= m_frameId)
        return false;
    if (!m_sync->lock())
        return false;

    const quint32 slotIndex = static_cast<quint32>(frameId % CameraIpc::kRingSlots);
    if (!writePlane(m_shm->data(), slotIndex, frameId, frame))
    {
        m_sync->unlock();
        return false;
    }

    auto *ctrl = CameraIpc::controlBlock(m_shm->data());
    ctrl->magic = CameraIpc::kMagic;
    ctrl->version = CameraIpc::kVersion;
    ctrl->frameId = frameId;
    ctrl->timestampMs = frame.timestampMs;
    ctrl->slotIndex = slotIndex;
    resetInspectMetadata(ctrl);

    const QByteArray pathUtf8 = frame.sourcePath.toUtf8();
    std::memset(ctrl->sourcePath, 0, sizeof(ctrl->sourcePath));
    std::memcpy(ctrl->sourcePath, pathUtf8.constData(),
                static_cast<size_t>(qMin(pathUtf8.size(), static_cast<int>(sizeof(ctrl->sourcePath) - 1))));

    const QByteArray statusUtf8 = frame.cameraStatus.toUtf8();
    std::memset(ctrl->cameraStatus, 0, sizeof(ctrl->cameraStatus));
    std::memcpy(ctrl->cameraStatus, statusUtf8.constData(),
                static_cast<size_t>(qMin(statusUtf8.size(), static_cast<int>(sizeof(ctrl->cameraStatus) - 1))));

    if (CameraIpc::planeHeader(m_shm->data(), slotIndex)->frameId != frameId)
    {
        m_sync->unlock();
        return false;
    }

    m_frameId = frameId;
    m_sync->unlock();
    m_sync->notifyFrameReady();
    return true;
}
