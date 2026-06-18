#include "CameraIpcSubscriber.h"

#include "CameraIpcSubscriberWorker.h"
#include "CameraHmiSocketServer.h"

CameraIpcSubscriber::CameraIpcSubscriber(QObject *parent)
    : QObject(parent)
{
    m_worker = new CameraIpcSubscriberWorker;
    m_socketServer = new CameraHmiSocketServer;
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

    connect(m_socketServer, &CameraHmiSocketServer::listenReady, this, &CameraIpcSubscriber::listenReady);
    connect(m_socketServer, &CameraHmiSocketServer::listenFailed, this, &CameraIpcSubscriber::listenFailed);
    connect(m_socketServer, &CameraHmiSocketServer::clientConnected, this, &CameraIpcSubscriber::clientConnected);
    connect(m_socketServer, &CameraHmiSocketServer::clientDisconnected, this,
            &CameraIpcSubscriber::clientDisconnected);

    connect(m_socketServer, &CameraHmiSocketServer::readyReceived, m_worker,
            &CameraIpcSubscriberWorker::onCameraReady);

    connect(m_worker, &CameraIpcSubscriberWorker::frameReceived, this, &CameraIpcSubscriber::frameReceived);
    connect(m_worker, &CameraIpcSubscriberWorker::errorMessage, this, &CameraIpcSubscriber::errorMessage);
}

void CameraIpcSubscriber::startComm()
{
    if (m_thread.isRunning())
        return;
    m_thread.start();
}

CameraIpcSubscriber::~CameraIpcSubscriber()
{
    if (m_worker)
        m_worker->stop();
    m_thread.quit();
    m_thread.wait(3000);
}

void CameraIpcSubscriber::resetConnection()
{
    if (m_worker)
        m_worker->requestReset();
}

void CameraIpcSubscriber::releaseDisplaySlot(quint64 frameId)
{
    if (!m_worker || frameId == 0)
        return;
    QMetaObject::invokeMethod(m_worker, "releaseDisplaySlot", Qt::QueuedConnection, Q_ARG(quint64, frameId));
}
