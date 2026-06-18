#include "CameraPublishWorker.h"

#include "CameraIpcLayout.h"
#include "ImageSourceFactory.h"

#include <QJsonArray>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

namespace
{
constexpr int kMaxQueueDepthNonStrict = 2;
constexpr int kHmiConnectAttempts = 60;
constexpr int kHmiConnectTimeoutMs = 1000;
constexpr int kHmiConnectRetryMs = 500;
constexpr int kSlotAckTotalMs = 60000;
constexpr int kSlotAckAttemptMs = 3000;
constexpr int kSlotRetryMs = 500;
} // namespace

CameraPublishWorker::CameraPublishWorker(QObject *parent)
    : QObject(parent)
    , m_intervalTimer(new QTimer(this))
{
    m_intervalTimer->setSingleShot(true);
    connect(m_intervalTimer, &QTimer::timeout, this, &CameraPublishWorker::emitReadyForNextGrab);
}

CameraPublishWorker::~CameraPublishWorker()
{
    stopPublish();
}

bool CameraPublishWorker::configure(const QJsonObject &rootConfig)
{
    m_strictAccounting = rootConfig.value(QStringLiteral("strictSampleAccounting")).toBool(true);

    const QString imageSource = rootConfig.value(QStringLiteral("imageSource")).toString().toLower();
    m_intervalMs = 800;
    if (imageSource == QStringLiteral("gige"))
    {
        m_intervalMs = rootConfig.value(QStringLiteral("camera")).toObject().value(QStringLiteral("grabIntervalMs")).toInt(800);
    }
    else
    {
        m_intervalMs = rootConfig.value(QStringLiteral("synthetic")).toObject().value(QStringLiteral("intervalMs")).toInt(800);
        const QJsonArray stations = rootConfig.value(QStringLiteral("stations")).toArray();
        for (const QJsonValue &v : stations)
        {
            const QJsonObject st = v.toObject();
            if (st.value(QStringLiteral("stationId")).toInt() == 1)
            {
                m_intervalMs = st.value(QStringLiteral("intervalMs")).toInt(m_intervalMs);
                break;
            }
        }
    }
    if (m_intervalMs <= 0)
        m_intervalMs = 800;

    if (!m_publisher.initialize())
        return false;

    m_source = ImageSourceFactory::create(rootConfig);
    if (!m_source || !m_source->open(rootConfig))
        return false;

    return true;
}

bool CameraPublishWorker::startPublish()
{
    if (m_running)
        return true;

    if (m_strictAccounting)
    {
        for (int attempt = 0; attempt < kHmiConnectAttempts; ++attempt)
        {
            if (m_socketClient.connectToHmi(kHmiConnectTimeoutMs))
                break;
            QThread::msleep(static_cast<unsigned long>(kHmiConnectRetryMs));
        }
        if (!m_socketClient.isConnected())
        {
            emit logMessage(QStringLiteral("连接 HMI 相机控制套接字失败（GasketVision.CameraHmi.Control）"));
            return false;
        }
    }

    m_running = true;
    emit logMessage(QStringLiteral("发布线程已启动（SHM%1）")
                        .arg(m_strictAccounting ? QStringLiteral(" + 环形存储区反压 + 直写")
                                                  : QStringLiteral(" + 非严格队列")));
    if (m_strictAccounting)
        QMetaObject::invokeMethod(this, "captureAndPublishDirect", Qt::QueuedConnection);
    return true;
}

void CameraPublishWorker::stopPublish()
{
    m_running = false;
    if (m_intervalTimer)
        m_intervalTimer->stop();

    QMutexLocker lock(&m_queueMutex);
    m_pendingFrames.clear();
    m_processing = false;
    m_grantedFrameId = 0;

    m_socketClient.disconnectFromHmi();
    if (m_source)
        m_source->close();
}

void CameraPublishWorker::captureAndPublishDirect()
{
    if (!m_running || !m_strictAccounting || !m_source)
        return;

    if (!requestCaptureSlot())
    {
        QTimer::singleShot(kSlotRetryMs, this, &CameraPublishWorker::captureAndPublishDirect);
        return;
    }

    const quint64 frameId = m_grantedFrameId;
    if (!m_publisher.captureFromSource(frameId, *m_source))
    {
        emit logMessage(QStringLiteral("相机 SHM 直写失败（frameId=%1）").arg(frameId));
        m_grantedFrameId = 0;
        QTimer::singleShot(kSlotRetryMs, this, &CameraPublishWorker::captureAndPublishDirect);
        return;
    }

    if (!m_publisher.waitForAnnotated(frameId, 15000))
        emit logMessage(QStringLiteral("等待引擎标注超时（frameId=%1）").arg(frameId));

    m_grantedFrameId = 0;
    m_intervalTimer->start(m_intervalMs);
}

void CameraPublishWorker::emitReadyForNextGrab()
{
    if (!m_running)
        return;
    if (m_strictAccounting)
        QMetaObject::invokeMethod(this, "captureAndPublishDirect", Qt::QueuedConnection);
    else
        emit readyForNextGrab();
}

bool CameraPublishWorker::canWriteSlotLocally(quint64 nextFrameId) const
{
    if (nextFrameId == 0)
        return false;
    if (nextFrameId <= CameraIpc::kRingSlots)
        return true;
    const quint64 blocker = nextFrameId - static_cast<quint64>(CameraIpc::kRingSlots);
    return m_socketClient.lastAckedFrameId() >= blocker;
}

bool CameraPublishWorker::requestCaptureSlot()
{
    if (!m_running || !m_strictAccounting)
        return true;

    const quint64 nextFrameId = m_publisher.lastPublishedFrameId() + 1;
    const quint32 slotIndex = static_cast<quint32>(nextFrameId % CameraIpc::kRingSlots);

    if (canWriteSlotLocally(nextFrameId))
    {
        m_grantedFrameId = nextFrameId;
        return true;
    }

    if (!m_socketClient.isConnected() && !m_socketClient.connectToHmi(kHmiConnectTimeoutMs))
        return false;

    if (!m_socketClient.sendReadyAndWaitAck(nextFrameId, slotIndex, kSlotAckTotalMs, kSlotAckAttemptMs))
    {
        emit logMessage(QStringLiteral("等待 HMI 采图空位失败（frameId=%1 slot=%2，已重试）")
                            .arg(nextFrameId)
                            .arg(slotIndex));
        return false;
    }

    m_grantedFrameId = nextFrameId;
    return true;
}

bool CameraPublishWorker::publishFrame(const VisionFrame &frame)
{
    if (m_strictAccounting)
    {
        if (m_grantedFrameId == 0)
        {
            emit logMessage(QStringLiteral("严格模式缺少 granted frameId"));
            return false;
        }
        if (!m_publisher.publish(frame, m_grantedFrameId))
        {
            emit logMessage(QStringLiteral("相机 SHM 发布失败"));
            return false;
        }
        if (!m_publisher.waitForAnnotated(m_grantedFrameId, 15000))
            emit logMessage(QStringLiteral("等待引擎标注超时（frameId=%1）").arg(m_grantedFrameId));
        m_grantedFrameId = 0;
        return true;
    }

    if (!m_publisher.publish(frame))
    {
        emit logMessage(QStringLiteral("相机 SHM 发布失败"));
        return false;
    }
    return true;
}

void CameraPublishWorker::processNextQueued()
{
    VisionFrame frame;
    {
        QMutexLocker lock(&m_queueMutex);
        if (!m_running || m_pendingFrames.isEmpty())
        {
            m_processing = false;
            return;
        }
        frame = m_pendingFrames.dequeue();
    }

    if (m_strictAccounting && !requestCaptureSlot())
    {
        {
            QMutexLocker lock(&m_queueMutex);
            m_pendingFrames.prepend(frame);
            m_processing = false;
        }
        QTimer::singleShot(kSlotRetryMs, this, [this]() {
            QMutexLocker lock(&m_queueMutex);
            if (!m_running || m_processing || m_pendingFrames.isEmpty())
                return;
            m_processing = true;
            QMetaObject::invokeMethod(this, "processNextQueued", Qt::QueuedConnection);
        });
        return;
    }

    const bool published = publishFrame(frame);

    if (m_strictAccounting)
    {
        if (!published)
        {
            QMutexLocker lock(&m_queueMutex);
            m_pendingFrames.prepend(frame);
        }

        QMutexLocker lock(&m_queueMutex);
        if (m_pendingFrames.isEmpty())
            m_processing = false;
        else
            QMetaObject::invokeMethod(this, "processNextQueued", Qt::QueuedConnection);
    }
    else
    {
        QMutexLocker lock(&m_queueMutex);
        if (m_pendingFrames.isEmpty())
            m_processing = false;
        else
            QMetaObject::invokeMethod(this, "processNextQueued", Qt::QueuedConnection);
    }
}

void CameraPublishWorker::onFrameGrabbed(const VisionFrame &frame)
{
    if (!m_running)
        return;

    QMutexLocker lock(&m_queueMutex);
    if (m_strictAccounting)
    {
        if (m_pendingFrames.size() >= CameraIpc::kRingSlots)
            return;
    }
    else if (m_pendingFrames.size() >= kMaxQueueDepthNonStrict)
    {
        return;
    }

    m_pendingFrames.enqueue(frame);
    if (m_processing)
        return;

    m_processing = true;
    QMetaObject::invokeMethod(this, "processNextQueued", Qt::QueuedConnection);
}
