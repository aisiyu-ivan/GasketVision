#ifndef HMIIPCSUBSCRIBER_H
#define HMIIPCSUBSCRIBER_H

#include "InspectionResult.h"

#include <QObject>
#include <QThread>

class EngineHmiSocketServer;
class HmiIpcSubscriberWorker;

// HMI 通信子线程：读检测 SHM + Engine 本地套接字（同一线程）
class HmiIpcSubscriber : public QObject
{
    Q_OBJECT

public:
    explicit HmiIpcSubscriber(QObject *parent = nullptr);
    ~HmiIpcSubscriber() override;

    // 主线程调用：连接完信号后再 startComm()
    void startComm();

    void resetConnection();

signals:
    void frameReceived(const InspectionResult &result);
    void errorMessage(const QString &message);
    void listenReady();
    void listenFailed(const QString &reason);
    void clientConnected();
    void clientDisconnected();

private:
    QThread m_thread;
    HmiIpcSubscriberWorker *m_worker = nullptr;
    EngineHmiSocketServer *m_socketServer = nullptr;
};

#endif // HMIIPCSUBSCRIBER_H
