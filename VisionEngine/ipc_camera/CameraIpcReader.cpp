#include "CameraIpcReader.h"

#include "CameraIpcLayout.h"
#include "CameraIpcSync.h"

#include <QDateTime>
#include <QSharedMemory>
#include <QThread>

#include <QtGlobal>

#include <opencv2/core.hpp>

#include <cstring>

namespace
{
void copyUtf8Field(char *dest, size_t destSize, const QString &text)
{
    const QByteArray utf8 = text.toUtf8();
    std::memset(dest, 0, destSize);
    std::memcpy(dest, utf8.constData(),
                static_cast<size_t>(qMin(utf8.size(), static_cast<int>(destSize - 1))));
}
} // namespace

CameraIpcReader::CameraIpcReader()
    : m_sync(new CameraIpcSync())
{
}

CameraIpcReader::~CameraIpcReader()
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

bool CameraIpcReader::attachRegion()
{
    if (m_shm && m_shm->isAttached())
        return true;

    if (m_shm)
    {
        if (m_shm->isAttached())
            m_shm->detach();
        delete m_shm;
        m_shm = nullptr;
    }

    if (!m_sync->open())
        return false;

    m_shm = new QSharedMemory(CameraIpc::shmKey());
    if (!m_shm->attach())
    {
        delete m_shm;
        m_shm = nullptr;
        return false;
    }
    if (m_shm->size() < static_cast<int>(CameraIpc::regionBytes()))
    {
        m_shm->detach();
        delete m_shm;
        m_shm = nullptr;
        return false;
    }
    return true;
}

bool CameraIpcReader::initialize()
{
    if (m_ready)
        return true;
    if (!attachRegion())
        return false;
    m_ready = true;
    return true;
}

bool CameraIpcReader::waitForRegion(int timeoutMs)
{
    if (m_ready)
        return true;

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMax(timeoutMs, 1);
    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        if (attachRegion())
        {
            m_ready = true;
            return true;
        }
        QThread::msleep(200);
    }
    return false;
}

CameraIpc::DefectCode CameraIpcReader::defectCodeFromText(const QString &defect)
{
    if (defect.isEmpty())
        return CameraIpc::DefectNone;
    if (defect == QStringLiteral("缺件"))
        return CameraIpc::DefectMissing;
    if (defect == QStringLiteral("尺寸超差"))
        return CameraIpc::DefectSize;
    if (defect == QStringLiteral("偏心偏移"))
        return CameraIpc::DefectEccentric;
    return CameraIpc::DefectUnknown;
}

bool CameraIpcReader::readFrame(VisionFrame &out)
{
    if (!attachRegion() || !m_sync->lock())
        return false;

    const auto *ctrl = CameraIpc::controlBlock(m_shm->data());
    if (ctrl->magic != CameraIpc::kMagic || ctrl->version != CameraIpc::kVersion || ctrl->frameId == 0
        || ctrl->frameId == m_lastFrameId)
    {
        m_sync->unlock();
        return false;
    }

    if (!(ctrl->flags & CameraIpc::kFlagHasOriginal) || (ctrl->flags & CameraIpc::kFlagHasAnnotated))
    {
        m_sync->unlock();
        return false;
    }

    const quint32 slot = ctrl->slotIndex % CameraIpc::kRingSlots;
    const auto *header = CameraIpc::planeHeader(m_shm->data(), slot);
    const uchar *payload = CameraIpc::planePayload(m_shm->data(), slot);

    if (header->frameId != ctrl->frameId || header->magic != CameraIpc::kMagic || header->payloadSize == 0
        || header->format != CameraIpc::Gray8)
    {
        m_sync->unlock();
        return false;
    }

    out.image = cv::Mat(static_cast<int>(header->height), static_cast<int>(header->width), CV_8UC1,
                        const_cast<uchar *>(payload), static_cast<size_t>(header->bytesPerLine));
    out.sourcePath = QString::fromUtf8(ctrl->sourcePath);
    out.timestampMs = ctrl->timestampMs;
    out.cameraStatus = QString::fromUtf8(ctrl->cameraStatus);
    out.frameId = ctrl->frameId;
    out.slotIndex = slot;
    m_lastFrameId = ctrl->frameId;

    m_sync->unlock();
    return true;
}

bool CameraIpcReader::prepareAnnotTarget(quint32 slotIndex, quint64 frameId, CameraAnnotTarget &out)
{
    if (!attachRegion())
        return false;

    out.frameId = frameId;
    out.slotIndex = slotIndex;
    out.header = CameraIpc::planeHeader(m_shm->data(), slotIndex);
    out.payload = CameraIpc::planePayload(m_shm->data(), slotIndex);
    out.capacity = static_cast<size_t>(CameraIpc::kMaxImageBytes);
    return out.header && out.payload;
}

bool CameraIpcReader::finishInspectPublish(int stationId, const GasketInspectResult &result,
                                           const QString &cameraStatus, quint64 frameId, quint32 slotIndex)
{
    if (!attachRegion() || !m_sync->lock())
        return false;

    const auto *header = CameraIpc::planeHeader(m_shm->data(), slotIndex);
    if (header->frameId != frameId || header->magic != CameraIpc::kMagic || header->payloadSize == 0
        || header->format != CameraIpc::Bgr888)
    {
        m_sync->unlock();
        return false;
    }

    auto *ctrl = CameraIpc::controlBlock(m_shm->data());
    const bool isCurrentFrame = ctrl->frameId == frameId;

    if (isCurrentFrame)
    {
        ctrl->stationId = static_cast<quint32>(stationId);
        ctrl->ok = result.ok ? 1 : 0;
        ctrl->defectCode = static_cast<quint8>(defectCodeFromText(result.defect));
        ctrl->flags = CameraIpc::kFlagHasOriginal | CameraIpc::kFlagHasAnnotated;
        ctrl->outerDiameterMm = result.outerDiameterMm;
        ctrl->innerDiameterMm = result.innerDiameterMm;
        ctrl->offsetXMm = result.offsetXMm;
        ctrl->offsetYMm = result.offsetYMm;
        ctrl->matchScore = result.matchScore;

        copyUtf8Field(ctrl->defectText, sizeof(ctrl->defectText), result.defect);
        copyUtf8Field(ctrl->cameraStatus, sizeof(ctrl->cameraStatus), cameraStatus);
        copyUtf8Field(ctrl->annotatedPath, sizeof(ctrl->annotatedPath),
                      QStringLiteral("captures/%1_annot.png").arg(frameId));
    }

    m_sync->unlock();
    if (isCurrentFrame)
        m_sync->notifyFrameReady();
    return true;
}

bool CameraIpcReader::waitAndRead(VisionFrame &out, int timeoutMs)
{
    if (!m_ready && !initialize())
        return false;

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMax(timeoutMs, 1);
    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        const int remaining = static_cast<int>(deadline - QDateTime::currentMSecsSinceEpoch());
        if (!m_sync->waitFrameReady(qMax(remaining, 1)))
            return false;

        if (readFrame(out))
            return true;
    }
    return false;
}
