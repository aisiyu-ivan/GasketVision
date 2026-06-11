#include "HmiIpcSubscriberWorker.h"

#include "EngineHmiSocketServer.h"
#include "HmiIpcLayout.h"
#include "HmiIpcSync.h"

#include <QDateTime>
#include <QImage>
#include <QSharedMemory>
#include <QThread>
#include <QTimer>

namespace
{
constexpr int kPollIntervalMs = 10;
constexpr int kReadRetryMs = 100;

bool hasSlotForFrame(quint64 frameId, quint64 lastDisplayedFrameId)
{
    if (frameId == 0)
        return false;
    return frameId <= lastDisplayedFrameId + static_cast<quint64>(HmiIpc::kRingSlots);
}
} // namespace

QString defectFromCode(quint8 code, const char *text)
{
    switch (code)
    {
    case HmiIpc::DefectMissing:
        return QStringLiteral("缺件");
    case HmiIpc::DefectSize:
        return QStringLiteral("尺寸超差");
    case HmiIpc::DefectEccentric:
        return QStringLiteral("偏心偏移");
    default:
        break;
    }
    if (text && text[0] != '\0')
        return QString::fromUtf8(text);
    return QString();
}

HmiIpcSubscriberWorker::HmiIpcSubscriberWorker(QObject *parent)
    : QObject(parent)
    , m_sync(new HmiIpcSync())
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &HmiIpcSubscriberWorker::onPollFrame);
}

void HmiIpcSubscriberWorker::setSocketServer(EngineHmiSocketServer *server)
{
    m_socketServer = server;
}

bool HmiIpcSubscriberWorker::attachRegion()
{
    if (m_shm && m_shm->isAttached())
        return true;

    if (!m_sync->open())
        return false;

    m_shm = new QSharedMemory(HmiIpc::shmKey());
    if (!m_shm->attach())
        return false;
    if (m_shm->size() < static_cast<int>(HmiIpc::regionBytes()))
        return false;
    return true;
}

void HmiIpcSubscriberWorker::releaseAttachedRegion()
{
    if (m_shm)
    {
        if (m_shm->isAttached())
            m_shm->detach();
        delete m_shm;
        m_shm = nullptr;
    }
    if (m_sync && m_sync->isOpen())
        m_sync->close();
    m_lastFrameId = 0;
    m_pendingReadyFrameId = 0;
    m_waitingLogged = false;
}

void HmiIpcSubscriberWorker::requestReset()
{
    m_resetRequested.store(true, std::memory_order_relaxed);
}

QImage HmiIpcSubscriberWorker::planeToImage(const HmiIpc::ImagePlaneHeader *header, const uchar *payload)
{
    if (!header || !payload || header->magic != HmiIpc::kMagic || header->payloadSize == 0)
        return QImage();

    QImage view;
    if (header->format == HmiIpc::Gray8)
    {
        view = QImage(payload, static_cast<int>(header->width), static_cast<int>(header->height),
                      static_cast<int>(header->bytesPerLine), QImage::Format_Grayscale8);
    }
    else if (header->format == HmiIpc::Bgr888)
    {
        view = QImage(payload, static_cast<int>(header->width), static_cast<int>(header->height),
                      static_cast<int>(header->bytesPerLine), QImage::Format_BGR888);
    }
    else
    {
        return QImage();
    }
    return view.copy();
}

bool HmiIpcSubscriberWorker::readFrame(InspectionResult &out)
{
    if (!attachRegion() || !m_sync->lock(500))
        return false;

    const auto *ctrl = HmiIpc::controlBlock(m_shm->data());
    if (ctrl->magic != HmiIpc::kMagic || ctrl->frameId == 0 || ctrl->frameId == m_lastFrameId)
    {
        m_sync->unlock();
        return false;
    }

    const quint32 slot = ctrl->slotIndex % HmiIpc::kRingSlots;
    const auto *origHeader = HmiIpc::originalHeader(m_shm->data(), slot);
    const auto *annHeader = HmiIpc::annotatedHeader(m_shm->data(), slot);

    if (origHeader->frameId != ctrl->frameId)
    {
        m_sync->unlock();
        return false;
    }
    if ((ctrl->flags & HmiIpc::kFlagHasAnnotated) && annHeader->frameId != ctrl->frameId)
    {
        m_sync->unlock();
        return false;
    }

    out = InspectionResult();
    out.fromIpc = true;
    out.frameId = ctrl->frameId;
    out.stationId = static_cast<int>(ctrl->stationId);
    out.ok = ctrl->ok != 0;
    out.defect = defectFromCode(ctrl->defectCode, ctrl->defectText);
    out.imagePath = QString::fromUtf8(ctrl->imagePath);
    out.annotatedImagePath = QString::fromUtf8(ctrl->annotatedPath);
    out.cameraStatus = QString::fromUtf8(ctrl->cameraStatus);
    out.measurements.outerDiameterMm = ctrl->outerDiameterMm;
    out.measurements.innerDiameterMm = ctrl->innerDiameterMm;
    out.measurements.offsetXMm = ctrl->offsetXMm;
    out.measurements.offsetYMm = ctrl->offsetYMm;
    out.measurements.matchScore = ctrl->matchScore;

    if (ctrl->flags & HmiIpc::kFlagHasOriginal)
    {
        out.originalImage =
            planeToImage(origHeader, HmiIpc::originalPayload(m_shm->data(), slot));
    }
    if (ctrl->flags & HmiIpc::kFlagHasAnnotated)
    {
        out.annotatedImage =
            planeToImage(annHeader, HmiIpc::annotatedPayload(m_shm->data(), slot));
    }

    m_lastFrameId = ctrl->frameId;
    m_sync->unlock();
    return true;
}

void HmiIpcSubscriberWorker::tryAckPendingReadyRequests()
{
    if (m_pendingReadyFrameId == 0 || !m_socketServer)
        return;
    if (!hasSlotForFrame(m_pendingReadyFrameId, m_lastFrameId))
        return;

    const quint64 ackId = m_pendingReadyFrameId;
    m_pendingReadyFrameId = 0;
    m_socketServer->sendAck(ackId);
}

void HmiIpcSubscriberWorker::onEngineReady(quint64 frameId, int, bool)
{
    m_pendingReadyFrameId = frameId;
    tryAckPendingReadyRequests();
}

void HmiIpcSubscriberWorker::onPollFrame()
{
    if (!m_running.load(std::memory_order_relaxed))
        return;

    if (m_resetRequested.exchange(false, std::memory_order_relaxed))
        releaseAttachedRegion();

    tryAckPendingReadyRequests();

    if (!attachRegion())
    {
        releaseAttachedRegion();
        if (!m_waitingLogged)
        {
            emit errorMessage(QStringLiteral("等待 VisionEngine 就绪…"));
            m_waitingLogged = true;
        }
        return;
    }

    if (m_waitingLogged)
    {
        m_waitingLogged = false;
        emit errorMessage(QStringLiteral("VisionEngine IPC 已连接"));
    }

    if (!m_sync->waitFrameReady(0))
        return;

    if (m_resetRequested.load(std::memory_order_relaxed))
        return;

    InspectionResult result;
    if (readFrame(result))
    {
        emit frameReceived(result);
        tryAckPendingReadyRequests();
        return;
    }

    const qint64 retryUntil = QDateTime::currentMSecsSinceEpoch() + kReadRetryMs;
    while (QDateTime::currentMSecsSinceEpoch() < retryUntil)
    {
        if (m_resetRequested.load(std::memory_order_relaxed))
            break;
        if (readFrame(result))
        {
            emit frameReceived(result);
            tryAckPendingReadyRequests();
            break;
        }
        QThread::msleep(5);
    }
}

void HmiIpcSubscriberWorker::start()
{
    m_running.store(true, std::memory_order_relaxed);
    m_waitingLogged = false;
    m_pollTimer->start();
}

void HmiIpcSubscriberWorker::stop()
{
    m_running.store(false, std::memory_order_relaxed);
    if (m_pollTimer)
        m_pollTimer->stop();
}

void HmiIpcSubscriberWorker::resetConnection()
{
    requestReset();
}
