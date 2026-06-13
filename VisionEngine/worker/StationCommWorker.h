#ifndef STATIONCOMMWORKER_H
#define STATIONCOMMWORKER_H

#include "GasketInspector.h"
#include "CameraEngineSocketServer.h"
#include "CameraIpcReader.h"
#include "EngineHmiSocketClient.h"
#include "HmiIpcPublisher.h"
#include "VisionFrame.h"

#include <QJsonObject>
#include <QObject>
#include <QQueue>

class QTimer;
class StationAlgoWorker;

// 通信线程：相机/HMI SHM、本地套接字、心跳 ping、帧管道调度
class StationCommWorker : public QObject
{
    Q_OBJECT

public:
    explicit StationCommWorker(QObject *parent = nullptr);
    ~StationCommWorker() override;

    void setStrictAccounting(bool strict) { m_strictAccounting = strict; }

    // 初始化 HMI IPC 并绑定算法线程对象
    bool configure(const QJsonObject &rootConfig, int stationId, StationAlgoWorker *algoWorker,
                   const QString &configDir = QString());

public slots:
    bool startComm();
    void stopComm();
    bool waitUntilReady();
    // 启动相机 SHM 轮询管道（须在通信线程调用）
    bool startPipeline();
    void stopPipeline();

public slots:
    void onInspectCompleted(const GasketInspectResult &result, const VisionFrame &frame);

signals:
    void logMessage(const QString &message);

private slots:
    void onPingTimer();
    void onPollFrame();
    void onReadyTick();

private:
    void tryDispatchInspect();
    bool requestHmiPublishSlot(quint64 hmiFrameId, int stationId, bool ok);

    bool m_strictAccounting = true;
    bool m_pipelineRunning = false;
    int m_stationId = 1;
    bool m_inspectRunning = false;
    StationAlgoWorker *m_algoWorker = nullptr;
    EngineHmiSocketClient m_hmiClient;
    CameraEngineSocketServer m_cameraServer;
    CameraIpcReader m_cameraReader;
    HmiIpcPublisher m_hmiIpc;
    QQueue<VisionFrame> m_pendingInspect;
    QTimer *m_pingTimer = nullptr;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_readyTimer = nullptr;
};

#endif // STATIONCOMMWORKER_H
