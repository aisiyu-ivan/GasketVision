#ifndef STATIONCOMMWORKER_H
#define STATIONCOMMWORKER_H

#include "GasketInspector.h"
#include "CameraIpcReader.h"
#include "VisionFrame.h"

#include <QJsonObject>
#include <QObject>
#include <QQueue>

class QTimer;
class StationAlgoWorker;

// 通信线程：读相机 SHM、调度算法标注（无套接字）
class StationCommWorker : public QObject
{
    Q_OBJECT

public:
    explicit StationCommWorker(QObject *parent = nullptr);
    ~StationCommWorker() override;

    void setStrictAccounting(bool strict) { m_strictAccounting = strict; }

    bool configure(const QJsonObject &rootConfig, int stationId, StationAlgoWorker *algoWorker,
                   const QString &configDir = QString());

public slots:
    bool startComm();
    void stopComm();
    bool startPipeline();
    void stopPipeline();

public slots:
    void onInspectCompleted(const GasketInspectResult &result, const VisionFrame &frame);

signals:
    void logMessage(const QString &message);

private slots:
    void onPollFrame();

private:
    void tryDispatchInspect();
    void finishInspectAndNotify(const GasketInspectResult &result, const VisionFrame &frame);

    bool m_strictAccounting = true;
    bool m_pipelineRunning = false;
    int m_stationId = 1;
    bool m_inspectRunning = false;
    StationAlgoWorker *m_algoWorker = nullptr;
    CameraIpcReader m_cameraReader;
    QQueue<VisionFrame> m_pendingInspect;
    QTimer *m_pollTimer = nullptr;
};

#endif // STATIONCOMMWORKER_H
