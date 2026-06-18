#include "CameraIpcSubscriberWorker.h"



#include "CameraIpcLayout.h"

#include "CameraIpcSync.h"

#include "CameraHmiSocketServer.h"



#include <QDateTime>

#include <QImage>

#include <QSharedMemory>

#include <QThread>

#include <QTimer>



namespace

{

constexpr int kPollIntervalMs = 10;

constexpr int kReadRetryMs = 100;



bool hasSlotForFrame(quint64 frameId, quint64 lastReleasedFrameId)

{

    if (frameId == 0)

        return false;

    return frameId <= lastReleasedFrameId + static_cast<quint64>(CameraIpc::kRingSlots);

}

} // namespace



QString defectFromCode(quint8 code, const char *text)

{

    switch (code)

    {

    case CameraIpc::DefectMissing:

        return QStringLiteral("缺件");

    case CameraIpc::DefectSize:

        return QStringLiteral("尺寸超差");

    case CameraIpc::DefectEccentric:

        return QStringLiteral("偏心偏移");

    default:

        break;

    }

    if (text && text[0] != '\0')

        return QString::fromUtf8(text);

    return QString();

}



CameraIpcSubscriberWorker::CameraIpcSubscriberWorker(QObject *parent)

    : QObject(parent)

    , m_sync(new CameraIpcSync())

    , m_pollTimer(new QTimer(this))

{

    m_pollTimer->setInterval(kPollIntervalMs);

    connect(m_pollTimer, &QTimer::timeout, this, &CameraIpcSubscriberWorker::onPollFrame);

}



void CameraIpcSubscriberWorker::setSocketServer(CameraHmiSocketServer *server)

{

    m_socketServer = server;

}



bool CameraIpcSubscriberWorker::attachRegion()
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



void CameraIpcSubscriberWorker::releaseAttachedRegion()

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

    m_lastReleasedFrameId = 0;

    m_pendingReadyFrameId = 0;

    m_pendingReadySlotIndex = 0;

    m_waitingLogged = false;

}



void CameraIpcSubscriberWorker::requestReset()

{

    m_resetRequested.store(true, std::memory_order_relaxed);

}



QImage CameraIpcSubscriberWorker::planeToImageView(const CameraIpc::ImagePlaneHeader *header, const uchar *payload)

{

    if (!header || !payload || header->magic != CameraIpc::kMagic || header->payloadSize == 0)

        return QImage();



    const int width = static_cast<int>(header->width);

    const int height = static_cast<int>(header->height);

    const int bytesPerLine = static_cast<int>(header->bytesPerLine);

    if (width <= 0 || height <= 0 || bytesPerLine < width * 3)

        return QImage();



    if (header->format == CameraIpc::Bgr888)

    {

        QImage rgb(width, height, QImage::Format_RGB888);

        for (int y = 0; y < height; ++y)

        {

            const uchar *src = payload + static_cast<qsizetype>(y) * bytesPerLine;

            uchar *dst = rgb.scanLine(y);

            for (int x = 0; x < width; ++x)

            {

                dst[x * 3 + 0] = src[x * 3 + 2];

                dst[x * 3 + 1] = src[x * 3 + 1];

                dst[x * 3 + 2] = src[x * 3 + 0];

            }

        }

        return rgb;

    }



    return QImage();

}



bool CameraIpcSubscriberWorker::readFrame(InspectionResult &out)

{

    if (!attachRegion() || !m_sync->lock(500))

        return false;



    const auto *ctrl = CameraIpc::controlBlock(m_shm->data());

    if (ctrl->magic != CameraIpc::kMagic || ctrl->version != CameraIpc::kVersion || ctrl->frameId == 0
         || ctrl->frameId == m_lastFrameId)
    {
        m_sync->unlock();
        return false;
    }

    if (!(ctrl->flags & CameraIpc::kFlagHasAnnotated))
    {
        m_sync->unlock();
        return false;
    }

    const quint32 slot = ctrl->slotIndex % CameraIpc::kRingSlots;

    const auto *header = CameraIpc::planeHeader(m_shm->data(), slot);



    if (header->frameId != ctrl->frameId || header->magic != CameraIpc::kMagic || header->payloadSize == 0
        || header->format != CameraIpc::Bgr888)
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

    out.imagePath = QString::fromUtf8(ctrl->sourcePath);

    out.annotatedImagePath = QString::fromUtf8(ctrl->annotatedPath);

    out.cameraStatus = QString::fromUtf8(ctrl->cameraStatus);

    out.measurements.outerDiameterMm = ctrl->outerDiameterMm;

    out.measurements.innerDiameterMm = ctrl->innerDiameterMm;

    out.measurements.offsetXMm = ctrl->offsetXMm;

    out.measurements.offsetYMm = ctrl->offsetYMm;

    out.measurements.matchScore = ctrl->matchScore;

    out.annotatedImage = planeToImageView(header, CameraIpc::planePayload(m_shm->data(), slot));

    if (out.annotatedImage.isNull())
    {
        m_sync->unlock();
        return false;
    }

    m_lastFrameId = ctrl->frameId;
    m_sync->unlock();
    return true;

}



void CameraIpcSubscriberWorker::releaseDisplaySlot(quint64 frameId)

{

    if (frameId == 0)

        return;

    if (m_socketServer && frameId > m_lastReleasedFrameId)

    {

        m_lastReleasedFrameId = frameId;

        const quint32 slotIndex = static_cast<quint32>(frameId % CameraIpc::kRingSlots);

        m_socketServer->sendAck(slotIndex, frameId);

    }

    tryAckPendingReadyRequests();

}



void CameraIpcSubscriberWorker::tryAckPendingReadyRequests()

{

    if (m_pendingReadyFrameId != 0 && m_socketServer

        && hasSlotForFrame(m_pendingReadyFrameId, m_lastReleasedFrameId))

    {

        const quint32 slot = m_pendingReadySlotIndex;

        const quint64 releasedId = m_lastReleasedFrameId;

        m_pendingReadyFrameId = 0;

        m_pendingReadySlotIndex = 0;

        m_socketServer->sendAck(slot, releasedId);

    }

}



void CameraIpcSubscriberWorker::onCameraReady(quint64 frameId, quint32 slotIndex)

{

    m_pendingReadyFrameId = frameId;

    m_pendingReadySlotIndex = slotIndex;

    tryAckPendingReadyRequests();

}



void CameraIpcSubscriberWorker::onPollFrame()

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

            emit errorMessage(QStringLiteral("等待 CameraService 就绪…"));

            m_waitingLogged = true;

        }

        return;

    }



    if (m_waitingLogged)
    {
        m_waitingLogged = false;
        emit errorMessage(QStringLiteral("相机 IPC 已连接"));
    }

    m_sync->waitFrameReady(0);

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



void CameraIpcSubscriberWorker::start()

{

    m_running.store(true, std::memory_order_relaxed);

    m_waitingLogged = false;

    m_pollTimer->start();

}



void CameraIpcSubscriberWorker::stop()

{

    m_running.store(false, std::memory_order_relaxed);

    if (m_pollTimer)

        m_pollTimer->stop();

}



void CameraIpcSubscriberWorker::resetConnection()

{

    requestReset();

}

