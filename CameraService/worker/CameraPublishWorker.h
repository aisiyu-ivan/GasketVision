#ifndef CAMERAPUBLISHWORKER_H
#define CAMERAPUBLISHWORKER_H

#include "CameraHmiSocketClient.h"
#include "CameraIpcPublisher.h"
#include "IVisionImageSource.h"
#include "VisionFrame.h"

#include <QJsonObject>
#include <QObject>

#include <QMutex>
#include <QQueue>

#include <memory>

class QTimer;

// 发布线程：写相机 SHM、严格模式环形存储区反压握手（Camera ↔ HMI 套接字）
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
    // 严格模式：写相机 SHM 前向 HMI 请求存储区空槽（慢路径）或本地判定（快路径）
    bool requestCaptureSlot();
    void onFrameGrabbed(const VisionFrame &frame);
    // 严格模式：ack 后采图直写 SHM（发布线程内完成）
    void captureAndPublishDirect();

signals:
    void readyForNextGrab();
    void logMessage(const QString &message);

private slots:
    void processNextQueued();
    void emitReadyForNextGrab();

private:
    bool publishFrame(const VisionFrame &frame);
    bool canWriteSlotLocally(quint64 nextFrameId) const;

    bool m_strictAccounting = true;
    bool m_running = false;
    int m_intervalMs = 800;
    quint64 m_grantedFrameId = 0;
    std::unique_ptr<IVisionImageSource> m_source;
    CameraIpcPublisher m_publisher;
    CameraHmiSocketClient m_socketClient;
    QMutex m_queueMutex;
    QQueue<VisionFrame> m_pendingFrames;
    bool m_processing = false;
    QTimer *m_intervalTimer = nullptr;
};

#endif // CAMERAPUBLISHWORKER_H
