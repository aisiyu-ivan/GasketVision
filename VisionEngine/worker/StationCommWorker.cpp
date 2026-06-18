#include "StationCommWorker.h"
#include "StationAlgoWorker.h"

#include "CameraAnnotTarget.h"
#include "CameraIpcLayout.h"
#include "InspectDispatch.h"

#include <QMetaObject>
#include <QTimer>

namespace
{
constexpr int kPollIntervalMs = 10;
constexpr int kCameraReadTimeoutMs = 200;
} // namespace

StationCommWorker::StationCommWorker(QObject *parent)
    : QObject(parent)
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &StationCommWorker::onPollFrame);
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
    return m_algoWorker != nullptr;
}

bool StationCommWorker::startComm()
{
    emit logMessage(QStringLiteral("通信线程已就绪（仅 SHM，无套接字）"));
    return true;
}

void StationCommWorker::stopComm()
{
    stopPipeline();
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

    if (!m_cameraReader.waitForRegion(60000))
    {
        emit logMessage(QStringLiteral("等待 CameraService 共享内存超时（请先启动 CameraService）"));
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

void StationCommWorker::tryDispatchInspect()
{
    if (!m_pipelineRunning || !m_algoWorker || m_inspectRunning || m_pendingInspect.isEmpty())
        return;

    const VisionFrame frame = m_pendingInspect.head();

    CameraAnnotTarget target;
    if (!m_cameraReader.prepareAnnotTarget(frame.slotIndex, frame.frameId, target))
    {
        emit logMessage(QStringLiteral("相机标注槽准备失败（frameId=%1）").arg(frame.frameId));
        return;
    }

    m_inspectRunning = true;
    const VisionFrame dequeued = m_pendingInspect.dequeue();
    InspectDispatch dispatch{dequeued, target};
    QMetaObject::invokeMethod(m_algoWorker, "inspectDispatchAsync", Qt::QueuedConnection,
                              Q_ARG(InspectDispatch, dispatch));
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

void StationCommWorker::finishInspectAndNotify(const GasketInspectResult &result, const VisionFrame &frame)
{
    if (!m_cameraReader.finishInspectPublish(m_stationId, result, frame.cameraStatus, frame.frameId, frame.slotIndex))
    {
        emit logMessage(QStringLiteral("相机 IPC 标注发布失败（frameId=%1）").arg(frame.frameId));
        m_inspectRunning = false;
        tryDispatchInspect();
        return;
    }

    m_inspectRunning = false;
    tryDispatchInspect();
}

void StationCommWorker::onInspectCompleted(const GasketInspectResult &result, const VisionFrame &frame)
{
    finishInspectAndNotify(result, frame);
}
