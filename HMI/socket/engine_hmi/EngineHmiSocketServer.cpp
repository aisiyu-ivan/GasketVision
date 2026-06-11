#include "EngineHmiSocketServer.h"

#include "EngineHmiProtocol.h"
#include "SocketLineCodec.h"

#include <QDateTime>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>
#include <QtLogging>

// 构造 Engine-HMI 本地套接字服务端
EngineHmiSocketServer::EngineHmiSocketServer(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
    , m_watchdog(new QTimer(this))
{
    connect(m_server, &QLocalServer::newConnection, this, &EngineHmiSocketServer::onNewConnection);
    m_watchdog->setInterval(2000);
    connect(m_watchdog, &QTimer::timeout, this, &EngineHmiSocketServer::checkHeartbeat);
}

// 释放监听与客户端连接
EngineHmiSocketServer::~EngineHmiSocketServer() = default;

// 在 GasketVision.EngineHmi.Control 上开始监听
bool EngineHmiSocketServer::listen()
{
    QLocalServer::removeServer(EngineHmiProtocol::serverName());
    if (!m_server->listen(EngineHmiProtocol::serverName()))
    {
        emit listenFailed(m_server->errorString());
        return false;
    }
    emit listenReady();
    return true;
}

// 向 VisionEngine 发送 frameId 确认行
bool EngineHmiSocketServer::sendAck(quint64 frameId)
{
    if (!m_client || m_client->state() != QLocalSocket::ConnectedState)
        return false;
    const QByteArray payload = SocketLineCodec::encodeLine(EngineHmiProtocol::ackLine(frameId));
    if (m_client->write(payload) != payload.size())
        return false;
    return m_client->waitForBytesWritten(3000);
}

// 供跨线程 invoke，在套接字线程发送 ack
bool EngineHmiSocketServer::deliverAck(quint64 frameId)
{
    return sendAck(frameId);
}

// 接受 VisionEngine 连接并启动心跳
void EngineHmiSocketServer::onNewConnection()
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
    connect(m_client, &QLocalSocket::readyRead, this, &EngineHmiSocketServer::onReadyRead);
    connect(m_client, &QLocalSocket::disconnected, this, &EngineHmiSocketServer::onDisconnected);
    m_watchdog->start();
    emit clientConnected();
}

// 读取套接字数据并解析行协议
void EngineHmiSocketServer::onReadyRead()
{
    if (!m_client)
        return;
    m_readBuffer += m_client->readAll();
    m_lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    parseBufferedLines();
}

// 从读缓冲中逐行解析 result 命令
void EngineHmiSocketServer::parseBufferedLines()
{
    QByteArray line;
    while (SocketLineCodec::takeLine(&m_readBuffer, &line))
    {
        if (line == "hello" || line == "ping")
            continue;

        quint64 frameId = 0;
        int stationId = 0;
        bool ok = false;
        if (EngineHmiProtocol::parseReady(line, &frameId, &stationId, &ok))
            emit readyReceived(frameId, stationId, ok);
        else if (EngineHmiProtocol::parseResult(line, &frameId, &stationId, &ok))
            emit resultReceived(frameId, stationId, ok);
    }
}

// 客户端断开时清理连接
void EngineHmiSocketServer::onDisconnected()
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

// 心跳超时则强制断开
void EngineHmiSocketServer::checkHeartbeat()
{
    if (!m_client)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastSeenMs > 15000)
    {
        qWarning("EngineHmi socket heartbeat timeout (lastSeen %lld ms ago)", now - m_lastSeenMs);
        m_client->disconnectFromServer();
        emit clientDisconnected();
    }
}
