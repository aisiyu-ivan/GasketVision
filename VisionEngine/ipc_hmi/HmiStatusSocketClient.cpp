#include "HmiStatusSocketClient.h"

#include <QLocalSocket>
#include <QTimer>

namespace
{
constexpr const char *kServerName = "GasketVision.Control";
} // namespace

// 构造状态套接字客户端
HmiStatusSocketClient::HmiStatusSocketClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_heartbeatTimer(new QTimer(this))
{
    m_heartbeatTimer->setInterval(1000);
    connect(m_heartbeatTimer, &QTimer::timeout, this, [this]() {
        if (m_socket->state() == QLocalSocket::ConnectedState)
            m_socket->write("ping\n");
    });
}

// 连接 HMI 状态套接字并启动定时 ping
bool HmiStatusSocketClient::connectToServer(int timeoutMs)
{
    m_socket->abort();
    m_socket->connectToServer(QString::fromLatin1(kServerName));
    if (!m_socket->waitForConnected(timeoutMs))
        return false;
    m_socket->write("hello\n");
    m_heartbeatTimer->start();
    return true;
}

// 断开连接并停止心跳
void HmiStatusSocketClient::disconnectFromServer()
{
    m_heartbeatTimer->stop();
    if (m_socket->state() != QLocalSocket::UnconnectedState)
        m_socket->disconnectFromServer();
}

// 当前是否已连接
bool HmiStatusSocketClient::isConnected() const
{
    return m_socket && m_socket->state() == QLocalSocket::ConnectedState;
}
