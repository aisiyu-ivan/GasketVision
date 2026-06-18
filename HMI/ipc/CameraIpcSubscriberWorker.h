#ifndef CAMERAIPCSUBSCRIBERWORKER_H
#define CAMERAIPCSUBSCRIBERWORKER_H

#include "CameraIpcLayout.h"
#include "InspectionResult.h"

#include <QObject>

#include <atomic>

class CameraHmiSocketServer;
class QTimer;

// HMI 通信线程：Camera ready/ack 槽位许可 + 读相机 SHM 标注图
class CameraIpcSubscriberWorker : public QObject
{
    Q_OBJECT

public:
    explicit CameraIpcSubscriberWorker(QObject *parent = nullptr);

    void setSocketServer(CameraHmiSocketServer *server);

    void requestReset();

public slots:
    void start();
    void stop();
    void resetConnection();
    void onCameraReady(quint64 frameId, quint32 slotIndex);
    void releaseDisplaySlot(quint64 frameId);

signals:
    void frameReceived(const InspectionResult &result);
    void errorMessage(const QString &message);

private slots:
    void onPollFrame();

private:
    bool attachRegion();
    void releaseAttachedRegion();
    bool readFrame(InspectionResult &out);
    void tryAckPendingReadyRequests();
    static QImage planeToImageView(const CameraIpc::ImagePlaneHeader *header, const uchar *payload);

    CameraHmiSocketServer *m_socketServer = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_resetRequested{false};
    bool m_waitingLogged = false;
    class QSharedMemory *m_shm = nullptr;
    class CameraIpcSync *m_sync = nullptr;
    quint64 m_lastFrameId = 0;
    quint64 m_lastReleasedFrameId = 0;
    quint64 m_pendingReadyFrameId = 0;
    quint32 m_pendingReadySlotIndex = 0;
    QTimer *m_pollTimer = nullptr;
};

#endif // CAMERAIPCSUBSCRIBERWORKER_H
