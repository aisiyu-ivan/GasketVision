#ifndef HMIIPCSUBSCRIBERWORKER_H
#define HMIIPCSUBSCRIBERWORKER_H

#include "HmiIpcLayout.h"
#include "InspectionResult.h"

#include <QObject>

#include <atomic>

class EngineHmiSocketServer;
class QTimer;

// HMI 通信线程：Engine ready/ack 环缓反压 + 读检测 SHM
class HmiIpcSubscriberWorker : public QObject
{
    Q_OBJECT

public:
    explicit HmiIpcSubscriberWorker(QObject *parent = nullptr);

    void setSocketServer(EngineHmiSocketServer *server);

    void requestReset();

public slots:
    void start();
    void stop();
    void resetConnection();
    void onEngineReady(quint64 frameId, int stationId, bool ok);

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
    static QImage planeToImage(const HmiIpc::ImagePlaneHeader *header, const uchar *payload);

    EngineHmiSocketServer *m_socketServer = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_resetRequested{false};
    bool m_waitingLogged = false;
    class QSharedMemory *m_shm = nullptr;
    class HmiIpcSync *m_sync = nullptr;
    quint64 m_lastFrameId = 0;
    quint64 m_pendingReadyFrameId = 0;
    QTimer *m_pollTimer = nullptr;
};

#endif // HMIIPCSUBSCRIBERWORKER_H
