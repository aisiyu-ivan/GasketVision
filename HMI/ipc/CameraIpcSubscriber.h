#ifndef CAMERAIPCSUBSCRIBER_H
#define CAMERAIPCSUBSCRIBER_H

#include "InspectionResult.h"

#include <QObject>
#include <QThread>

class CameraHmiSocketServer;
class CameraIpcSubscriberWorker;

// HMI 通信子线程：读相机 SHM + Camera 本地套接字（同一线程）
class CameraIpcSubscriber : public QObject
{
    Q_OBJECT

public:
    explicit CameraIpcSubscriber(QObject *parent = nullptr);
    ~CameraIpcSubscriber() override;

    void startComm();

    void resetConnection();
    void releaseDisplaySlot(quint64 frameId);

signals:
    void frameReceived(const InspectionResult &result);
    void errorMessage(const QString &message);
    void listenReady();
    void listenFailed(const QString &reason);
    void clientConnected();
    void clientDisconnected();

private:
    QThread m_thread;
    CameraIpcSubscriberWorker *m_worker = nullptr;
    CameraHmiSocketServer *m_socketServer = nullptr;
};

#endif // CAMERAIPCSUBSCRIBER_H
