#include "CameraHmiSocketServer.h"

#include "CameraHmiProtocol.h"
#include "SocketLineCodec.h"

#include <QDateTime>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QtLogging>

CameraHmiSocketServer::CameraHmiSocketServer(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
    , m_watchdog(new QTimer(this))
{
    connect(m_server, &QLocalServer::newConnection, this, &CameraHmiSocketServer::onNewConnection);
    m_watchdog->setInterval(2000);
    connect(m_watchdog, &QTimer::timeout, this, &CameraHmiSocketServer::checkHeartbeat);
}

CameraHmiSocketServer::~CameraHmiSocketServer() = default;

bool CameraHmiSocketServer::listen()
{
    QLocalServer::removeServer(CameraHmiProtocol::serverName());
    if (!m_server->listen(CameraHmiProtocol::serverName()))
    {
        emit listenFailed(m_server->errorString());
        return false;
    }
    emit listenReady();
    return true;
}

bool CameraHmiSocketServer::sendAck(quint32 slotIndex, quint64 releasedFrameId)
{
    if (!m_client || m_client->state() != QLocalSocket::ConnectedState)
        return false;
    const QByteArray payload = SocketLineCodec::encodeLine(CameraHmiProtocol::ackLine(slotIndex, releasedFrameId));
    if (m_client->write(payload) != payload.size())
        return false;
    return m_client->waitForBytesWritten(3000);
}

void CameraHmiSocketServer::onNewConnection()
{
    if (m_client)
    {
        m_client->disconnectFromServer();
        m_client->deleteLater();
        m_client = nullptr;
    }

    m_client = m_server->nextPendingConnection();
    if (!m_client)
        return;

    m_readBuffer.clear();
    m_lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    connect(m_client, &QLocalSocket::readyRead, this, &CameraHmiSocketServer::onReadyRead);
    connect(m_client, &QLocalSocket::disconnected, this, &CameraHmiSocketServer::onDisconnected);
    m_watchdog->start();
    emit clientConnected();
}

void CameraHmiSocketServer::onReadyRead()
{
    if (!m_client)
        return;
    m_readBuffer += m_client->readAll();
    m_lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    parseBufferedLines();
}

void CameraHmiSocketServer::parseBufferedLines()
{
    QByteArray line;
    while (SocketLineCodec::takeLine(&m_readBuffer, &line))
    {
        if (line == "hello" || line == "ping")
            continue;

        quint64 frameId = 0;
        quint32 slotIndex = 0;
        if (CameraHmiProtocol::parseReady(line, &frameId, &slotIndex))
            emit readyReceived(frameId, slotIndex);
    }
}

void CameraHmiSocketServer::onDisconnected()
{
    m_watchdog->stop();
    m_readBuffer.clear();
    if (m_client)
    {
        m_client->deleteLater();
        m_client = nullptr;
    }
    emit clientDisconnected();
}

void CameraHmiSocketServer::checkHeartbeat()
{
    if (!m_client)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastSeenMs > 15000)
    {
        qWarning("CameraHmi socket heartbeat timeout (lastSeen %lld ms ago)", now - m_lastSeenMs);
        m_client->disconnectFromServer();
        emit clientDisconnected();
    }
}
