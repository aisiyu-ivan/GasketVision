#include "HmiIpcSubscriber.h"

#include "EngineHmiSocketServer.h"
#include "HmiIpcSubscriberWorker.h"

// 启动通信子线程（SHM + 套接字）
HmiIpcSubscriber::HmiIpcSubscriber(QObject *parent)
    : QObject(parent)
{
    m_worker = new HmiIpcSubscriberWorker;
    m_socketServer = new EngineHmiSocketServer;
    m_worker->setSocketServer(m_socketServer);

    m_worker->moveToThread(&m_thread);
    m_socketServer->moveToThread(&m_thread);

    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(&m_thread, &QThread::finished, m_socketServer, &QObject::deleteLater);

    connect(&m_thread, &QThread::started, m_socketServer, [this]() {
        if (!m_socketServer->listen())
            return;
        m_worker->start();
    });

    connect(m_socketServer, &EngineHmiSocketServer::listenReady, this, &HmiIpcSubscriber::listenReady);
    connect(m_socketServer, &EngineHmiSocketServer::listenFailed, this, &HmiIpcSubscriber::listenFailed);
    connect(m_socketServer, &EngineHmiSocketServer::clientConnected, this, &HmiIpcSubscriber::clientConnected);
    connect(m_socketServer, &EngineHmiSocketServer::clientDisconnected, this,
            &HmiIpcSubscriber::clientDisconnected);

    connect(m_socketServer, &EngineHmiSocketServer::readyReceived, m_worker,
            &HmiIpcSubscriberWorker::onEngineReady);

    connect(m_worker, &HmiIpcSubscriberWorker::frameReceived, this, &HmiIpcSubscriber::frameReceived);
    connect(m_worker, &HmiIpcSubscriberWorker::errorMessage, this, &HmiIpcSubscriber::errorMessage);
}

void HmiIpcSubscriber::startComm()
{
    if (m_thread.isRunning())
        return;
    m_thread.start();
}

HmiIpcSubscriber::~HmiIpcSubscriber()
{
    if (m_worker)
        m_worker->stop();
    m_thread.quit();
    m_thread.wait(3000);
}

void HmiIpcSubscriber::resetConnection()
{
    if (m_worker)
        m_worker->requestReset();
}
