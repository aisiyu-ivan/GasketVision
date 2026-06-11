#include "HmiIpcPublisher.h"

#include "HmiIpcLayout.h"
#include "HmiIpcSync.h"

#include <QDateTime>
#include <QSharedMemory>

#include <opencv2/core.hpp>

#include <cstring>

// 构造 HMI IPC 发布器
HmiIpcPublisher::HmiIpcPublisher()
    : m_shmKey(HmiIpc::shmKey())
    , m_sync(new HmiIpcSync())
{
}

// 释放 SHM 与同步对象
HmiIpcPublisher::~HmiIpcPublisher()
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

// 创建检测 SHM 区域与同步对象
bool HmiIpcPublisher::initialize()
{
    if (m_ready)
        return true;

    QSharedMemory stale(m_shmKey);
    if (stale.attach())
        stale.detach();

    if (!m_sync->create())
        return false;

    m_shm = new QSharedMemory(m_shmKey);
    const qsizetype bytes = HmiIpc::regionBytes();
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
    auto *ctrl = HmiIpc::controlBlock(m_shm->data());
    ctrl->magic = HmiIpc::kMagic;
    ctrl->version = HmiIpc::kVersion;

    m_ready = true;
    return true;
}

// 缺陷中文描述转协议枚举
HmiIpc::DefectCode HmiIpcPublisher::defectCodeFromText(const QString &defect)
{
    if (defect.isEmpty())
        return HmiIpc::DefectNone;
    if (defect == QStringLiteral("缺件"))
        return HmiIpc::DefectMissing;
    if (defect == QStringLiteral("尺寸超差"))
        return HmiIpc::DefectSize;
    if (defect == QStringLiteral("偏心偏移"))
        return HmiIpc::DefectEccentric;
    return HmiIpc::DefectUnknown;
}

// 将 Mat 写入环槽原图或标注图平面
bool HmiIpcPublisher::writePlane(void *shm, quint32 slotIndex, bool annotated, quint64 frameId,
                                 const cv::Mat &image)
{
    if (image.empty())
        return false;

    cv::Mat continuous = image.isContinuous() ? image : image.clone();
    quint8 format = HmiIpc::Gray8;
    if (continuous.channels() == 3)
        format = HmiIpc::Bgr888;
    else if (continuous.channels() != 1)
        return false;

    const quint32 payloadSize =
        static_cast<quint32>(static_cast<qsizetype>(continuous.step) * continuous.rows);
    if (payloadSize == 0 || payloadSize > static_cast<quint32>(HmiIpc::kMaxImageBytes))
        return false;

    HmiIpc::ImagePlaneHeader *header =
        annotated ? HmiIpc::annotatedHeader(shm, slotIndex) : HmiIpc::originalHeader(shm, slotIndex);
    uchar *payload =
        annotated ? HmiIpc::annotatedPayload(shm, slotIndex) : HmiIpc::originalPayload(shm, slotIndex);

    header->magic = HmiIpc::kMagic;
    header->frameId = frameId;
    header->width = static_cast<quint32>(continuous.cols);
    header->height = static_cast<quint32>(continuous.rows);
    header->bytesPerLine = static_cast<quint32>(continuous.step);
    header->format = format;
    header->payloadSize = payloadSize;
    std::memcpy(payload, continuous.data, payloadSize);
    return true;
}

// 发布检测结果（原图、标注图、测量值、OK/NG）
bool HmiIpcPublisher::publish(int stationId, const GasketInspectResult &result,
                              const cv::Mat &originalImage, const QString &cameraStatus)
{
    if (!m_ready && !initialize())
        return false;
    const quint64 frameId = m_frameId + 1;
    return publish(stationId, result, originalImage, cameraStatus, frameId);
}

// 严格模式：按指定 frameId 写入环槽
bool HmiIpcPublisher::publish(int stationId, const GasketInspectResult &result,
                              const cv::Mat &originalImage, const QString &cameraStatus,
                              quint64 frameId)
{
    if (!m_ready && !initialize())
        return false;
    if (frameId == 0 || frameId <= m_frameId)
        return false;
    if (!m_sync->lock())
        return false;

    const quint32 slotIndex = static_cast<quint32>(frameId % HmiIpc::kRingSlots);

    quint8 flags = 0;
    if (writePlane(m_shm->data(), slotIndex, false, frameId, originalImage))
        flags |= HmiIpc::kFlagHasOriginal;
    if (writePlane(m_shm->data(), slotIndex, true, frameId, result.annotated))
        flags |= HmiIpc::kFlagHasAnnotated;

    auto *ctrl = HmiIpc::controlBlock(m_shm->data());
    ctrl->magic = HmiIpc::kMagic;
    ctrl->version = HmiIpc::kVersion;
    ctrl->frameId = frameId;
    ctrl->timestampMs = QDateTime::currentMSecsSinceEpoch();
    ctrl->slotIndex = slotIndex;
    ctrl->stationId = static_cast<quint32>(stationId);
    ctrl->ok = result.ok ? 1 : 0;
    ctrl->defectCode = static_cast<quint8>(defectCodeFromText(result.defect));
    ctrl->flags = flags;
    ctrl->outerDiameterMm = result.outerDiameterMm;
    ctrl->innerDiameterMm = result.innerDiameterMm;
    ctrl->offsetXMm = result.offsetXMm;
    ctrl->offsetYMm = result.offsetYMm;
    ctrl->matchScore = result.matchScore;

    const QByteArray defectUtf8 = result.defect.toUtf8();
    std::memset(ctrl->defectText, 0, sizeof(ctrl->defectText));
    std::memcpy(ctrl->defectText, defectUtf8.constData(),
                static_cast<size_t>(qMin(defectUtf8.size(), static_cast<int>(sizeof(ctrl->defectText) - 1))));

    const QByteArray statusUtf8 = cameraStatus.toUtf8();
    std::memset(ctrl->cameraStatus, 0, sizeof(ctrl->cameraStatus));
    std::memcpy(ctrl->cameraStatus, statusUtf8.constData(),
                static_cast<size_t>(qMin(statusUtf8.size(), static_cast<int>(sizeof(ctrl->cameraStatus) - 1))));

    const QByteArray pathUtf8 = result.imagePath.toUtf8();
    std::memset(ctrl->imagePath, 0, sizeof(ctrl->imagePath));
    std::memcpy(ctrl->imagePath, pathUtf8.constData(),
                static_cast<size_t>(qMin(pathUtf8.size(), static_cast<int>(sizeof(ctrl->imagePath) - 1))));

    const QByteArray annUtf8 = result.annotatedImagePath.toUtf8();
    std::memset(ctrl->annotatedPath, 0, sizeof(ctrl->annotatedPath));
    std::memcpy(ctrl->annotatedPath, annUtf8.constData(),
                static_cast<size_t>(qMin(annUtf8.size(), static_cast<int>(sizeof(ctrl->annotatedPath) - 1))));

    const auto *origHeader = HmiIpc::originalHeader(m_shm->data(), slotIndex);
    const auto *annHeader = HmiIpc::annotatedHeader(m_shm->data(), slotIndex);
    const bool aligned = origHeader->frameId == frameId
                         && (!(flags & HmiIpc::kFlagHasAnnotated) || annHeader->frameId == frameId);
    if (!aligned)
    {
        m_sync->unlock();
        return false;
    }

    m_frameId = frameId;
    m_sync->unlock();
    m_sync->notifyFrameReady();
    return true;
}
