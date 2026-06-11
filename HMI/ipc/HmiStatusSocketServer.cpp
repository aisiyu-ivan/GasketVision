#include "HmiStatusSocketServer.h"

#include <QDateTime>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

namespace
{
constexpr const char *kServerName = "GasketVision.Control";
} // namespace

// 构造状态套接字服务端
HmiStatusSocketServer::HmiStatusSocketServer(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
    , m_watchdog(new QTimer(this))
{
    connect(m_server, &QLocalServer::newConnection, this, &HmiStatusSocketServer::onNewConnection);
    m_watchdog->setInterval(2000);
    connect(m_watchdog, &QTimer::timeout, this, &HmiStatusSocketServer::checkHeartbeat);
}

// 停止监听并释放客户端连接
HmiStatusSocketServer::~HmiStatusSocketServer() = default;

// 在 GasketVision.Control 上开始监听
bool HmiStatusSocketServer::listen()
{
    QLocalServer::removeServer(QString::fromLatin1(kServerName));
    if (!m_server->listen(QString::fromLatin1(kServerName)))
    {
        emit listenFailed(m_server->errorString());
        return false;
    }
    emit listenReady();
    return true;
}

// 接受 VisionEngine 连接
void HmiStatusSocketServer::onNewConnection()
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

    m_lastSeenMs = QDateTime::currentMSecsSinceEpoch();
    connect(m_client, &QLocalSocket::readyRead, this, &HmiStatusSocketServer::onReadyRead);
    connect(m_client, &QLocalSocket::disconnected, this, &HmiStatusSocketServer::onDisconnected);
    m_watchdog->start();
    emit clientConnected();
}

// 读取 hello/ping 并刷新心跳时间
void HmiStatusSocketServer::onReadyRead()
{
    if (!m_client)
        return;
    m_client->readAll();
    m_lastSeenMs = QDateTime::currentMSecsSinceEpoch();
}

// 客户端断开
void HmiStatusSocketServer::onDisconnected()
{
    m_watchdog->stop();
    if (m_client)
    {
        m_client->deleteLater();
        m_client = nullptr;
    }
    emit clientDisconnected();
}

// 心跳超时则强制断开
void HmiStatusSocketServer::checkHeartbeat()
{
    if (!m_client)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastSeenMs > 5000)
    {
        m_client->disconnectFromServer();
        emit clientDisconnected();
    }
}
