#include "StationCommWorker.h"
#include "StationAlgoWorker.h"

#include "CameraIpcLayout.h"

#include <QMetaObject>
#include <QTimer>

namespace
{
constexpr int kPingIntervalMs = 1000;
constexpr int kPollIntervalMs = 10;
constexpr int kReadyTickMs = 10;
constexpr int kCameraReadTimeoutMs = 200;
constexpr int kHmiSlotAckTotalMs = 60000;
constexpr int kHmiSlotAckAttemptMs = 3000;
constexpr int kHmiPublishRetryMs = 500;
} // namespace

StationCommWorker::StationCommWorker(QObject *parent)
    : QObject(parent)
    , m_hmiClient(this)
    , m_cameraServer()
    , m_pingTimer(new QTimer(this))
    , m_pollTimer(new QTimer(this))
    , m_readyTimer(new QTimer(this))
{
    m_pingTimer->setInterval(kPingIntervalMs);
    connect(m_pingTimer, &QTimer::timeout, this, &StationCommWorker::onPingTimer);

    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &StationCommWorker::onPollFrame);

    m_readyTimer->setInterval(kReadyTickMs);
    connect(m_readyTimer, &QTimer::timeout, this, &StationCommWorker::onReadyTick);
}

StationCommWorker::~StationCommWorker()
{
    stopComm();
}

bool StationCommWorker::configure(const QJsonObject &rootConfig, int stationId, StationAlgoWorker *algoWorker,
                                  const QString &)
{
    m_stationId = stationId;
    m_algoWorker = algoWorker;
    m_strictAccounting = rootConfig.value(QStringLiteral("strictSampleAccounting")).toBool(true);

    if (!m_hmiIpc.initialize())
    {
        emit logMessage(QStringLiteral("HMI IPC 共享内存初始化失败"));
        return false;
    }

    return true;
}

bool StationCommWorker::startComm()
{
    if (m_strictAccounting && !m_cameraServer.listen())
    {
        emit logMessage(QStringLiteral("Camera 本地套接字监听失败（GasketVision.Camera.Control）"));
        return false;
    }

    if (!m_hmiClient.connectToHmi())
    {
        emit logMessage(QStringLiteral("连接 HMI 本地套接字失败（GasketVision.EngineHmi.Control）"));
        return false;
    }

    emit logMessage(QStringLiteral("已连接 HMI 本地套接字（通信线程）"));
    m_pingTimer->start();
    if (m_strictAccounting)
        m_readyTimer->start();
    return true;
}

void StationCommWorker::stopComm()
{
    stopPipeline();
    if (m_pingTimer)
        m_pingTimer->stop();
    if (m_readyTimer)
        m_readyTimer->stop();
    m_hmiClient.disconnectFromHmi();
    m_cameraServer.close();
}

bool StationCommWorker::waitUntilReady()
{
    return m_hmiClient.isConnected() || m_hmiClient.ensureConnected();
}

bool StationCommWorker::startPipeline()
{
    if (m_pipelineRunning)
        return true;

    if (!m_algoWorker)
    {
        emit logMessage(QStringLiteral("算法线程未配置"));
        return false;
    }

    if (!waitUntilReady())
    {
        emit logMessage(QStringLiteral("HMI 本地套接字未就绪"));
        return false;
    }

    m_pipelineRunning = true;
    m_pollTimer->start();
    emit logMessage(QStringLiteral("帧管道已启动（通信线程），等待 CameraService…"));
    return true;
}

void StationCommWorker::stopPipeline()
{
    m_pipelineRunning = false;
    m_inspectRunning = false;
    m_pendingInspect.clear();
    if (m_pollTimer)
        m_pollTimer->stop();
}

void StationCommWorker::onReadyTick()
{
    if (!m_strictAccounting)
        return;
    m_cameraServer.processReadyRequests(m_cameraReader.lastConsumedFrameId());
}

bool StationCommWorker::requestHmiPublishSlot(quint64 hmiFrameId, int stationId, bool ok)
{
    if (!m_hmiClient.sendReadyAndWaitAck(hmiFrameId, stationId, ok, kHmiSlotAckTotalMs, kHmiSlotAckAttemptMs))
    {
        emit logMessage(QStringLiteral("等待 HMI 展示空位失败（frameId=%1，已重试）").arg(hmiFrameId));
        return false;
    }
    return true;
}

void StationCommWorker::onPingTimer()
{
    if (!m_hmiClient.isConnected())
    {
        if (m_hmiClient.ensureConnected())
            emit logMessage(QStringLiteral("HMI 本地套接字已重连"));
        return;
    }
    m_hmiClient.sendPing();
}

void StationCommWorker::tryDispatchInspect()
{
    if (!m_pipelineRunning || !m_algoWorker || m_inspectRunning || m_pendingInspect.isEmpty())
        return;

    m_inspectRunning = true;
    const VisionFrame frame = m_pendingInspect.dequeue();
    QMetaObject::invokeMethod(m_algoWorker, "inspectFrameAsync", Qt::QueuedConnection, Q_ARG(VisionFrame, frame));
}

void StationCommWorker::onPollFrame()
{
    if (!m_pipelineRunning || !m_algoWorker)
        return;

    if (m_pendingInspect.size() < CameraIpc::kRingSlots)
    {
        VisionFrame frame;
        if (m_cameraReader.waitAndRead(frame, kCameraReadTimeoutMs))
            m_pendingInspect.enqueue(frame);
    }

    tryDispatchInspect();
}

void StationCommWorker::onInspectCompleted(const GasketInspectResult &result, const VisionFrame &frame)
{
    if (m_strictAccounting)
    {
        const quint64 hmiFrameId = m_hmiIpc.lastPublishedFrameId() + 1;
        if (!requestHmiPublishSlot(hmiFrameId, m_stationId, result.ok))
        {
            QTimer::singleShot(kHmiPublishRetryMs, this, [this, result, frame]() {
                if (m_pipelineRunning)
                    onInspectCompleted(result, frame);
            });
            return;
        }

        if (!m_hmiIpc.publish(m_stationId, result, frame.image, frame.cameraStatus, hmiFrameId))
            emit logMessage(QStringLiteral("HMI IPC 发布失败"));
    }
    else
    {
        if (!m_hmiIpc.publish(m_stationId, result, frame.image, frame.cameraStatus))
            emit logMessage(QStringLiteral("HMI IPC 发布失败"));
    }

    m_inspectRunning = false;
    tryDispatchInspect();
}
