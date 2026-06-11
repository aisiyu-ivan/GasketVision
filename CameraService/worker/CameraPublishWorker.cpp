#include "CameraPublishWorker.h"

#include <QJsonArray>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

namespace
{
constexpr int kMaxQueueDepthNonStrict = 2;
constexpr int kEngineConnectAttempts = 60;
constexpr int kEngineConnectTimeoutMs = 1000;
constexpr int kEngineConnectRetryMs = 500;
constexpr int kSlotAckTotalMs = 60000;
constexpr int kSlotAckAttemptMs = 3000;
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

    return true;
}

bool CameraPublishWorker::startPublish()
{
    if (m_running)
        return true;

    if (m_strictAccounting)
    {
        for (int attempt = 0; attempt < kEngineConnectAttempts; ++attempt)
        {
            if (m_socketClient.connectToEngine(kEngineConnectTimeoutMs))
                break;
            QThread::msleep(static_cast<unsigned long>(kEngineConnectRetryMs));
        }
        if (!m_socketClient.isConnected())
        {
            emit logMessage(QStringLiteral("连接 VisionEngine 相机控制套接字失败"));
            return false;
        }
    }

    m_running = true;
    emit logMessage(QStringLiteral("发布线程已启动（SHM%s）")
                        .arg(m_strictAccounting ? QStringLiteral(" + 环缓反压") : QString()));
    if (m_strictAccounting)
        emit readyForNextGrab();
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

    m_socketClient.disconnectFromEngine();
}

void CameraPublishWorker::emitReadyForNextGrab()
{
    if (!m_running)
        return;
    emit readyForNextGrab();
}

bool CameraPublishWorker::requestCaptureSlot()
{
    if (!m_running || !m_strictAccounting)
        return true;
    if (!m_socketClient.isConnected() && !m_socketClient.connectToEngine(kEngineConnectTimeoutMs))
        return false;

    const quint64 nextFrameId = m_publisher.lastPublishedFrameId() + 1;
    if (!m_socketClient.sendReadyAndWaitAck(nextFrameId, kSlotAckTotalMs, kSlotAckAttemptMs))
    {
        emit logMessage(QStringLiteral("等待 Engine 采图空位失败（frameId=%1，已重试）").arg(nextFrameId));
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

    const bool published = publishFrame(frame);

    if (m_strictAccounting)
    {
        if (published)
            m_intervalTimer->start(m_intervalMs);
        else
            emit readyForNextGrab();

        QMutexLocker lock(&m_queueMutex);
        if (m_pendingFrames.isEmpty())
            m_processing = false;
    }
    else
    {
        QMutexLocker lock(&m_queueMutex);
        if (m_pendingFrames.isEmpty())
            m_processing = false;
        else
            QMetaObject::invokeMethod(this, "processNextQueued", Qt::QueuedConnection);
    }

    Q_UNUSED(published);
}

void CameraPublishWorker::onFrameGrabbed(const VisionFrame &frame)
{
    if (!m_running)
        return;

    QMutexLocker lock(&m_queueMutex);
    if (m_strictAccounting)
    {
        if (!m_pendingFrames.isEmpty())
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
