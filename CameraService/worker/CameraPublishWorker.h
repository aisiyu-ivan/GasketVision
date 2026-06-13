#ifndef CAMERAPUBLISHWORKER_H
#define CAMERAPUBLISHWORKER_H

#include "CameraEngineSocketClient.h"
#include "CameraIpcPublisher.h"
#include "VisionFrame.h"

#include <QJsonObject>
#include <QObject>

#include <QMutex>
#include <QQueue>

class QTimer;

// 发布线程：写相机 SHM、严格模式环缓反压握手
class CameraPublishWorker : public QObject
{
    Q_OBJECT

public:
    explicit CameraPublishWorker(QObject *parent = nullptr);
    ~CameraPublishWorker() override;

    bool configure(const QJsonObject &rootConfig);

public slots:
    bool startPublish();
    void stopPublish();
    // 严格模式：写相机 SHM 前向 Engine 请求环缓空位
    bool requestCaptureSlot();
    void onFrameGrabbed(const VisionFrame &frame);

signals:
    void readyForNextGrab();
    void logMessage(const QString &message);

private slots:
    void processNextQueued();
    void emitReadyForNextGrab();

private:
    bool publishFrame(const VisionFrame &frame);

    bool m_strictAccounting = true;
    bool m_running = false;
    int m_intervalMs = 800;
    quint64 m_grantedFrameId = 0;
    CameraIpcPublisher m_publisher;
    CameraEngineSocketClient m_socketClient;
    QMutex m_queueMutex;
    QQueue<VisionFrame> m_pendingFrames;
    bool m_processing = false;
    QTimer *m_intervalTimer = nullptr;
};

#endif // CAMERAPUBLISHWORKER_H
