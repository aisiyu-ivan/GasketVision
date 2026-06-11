#include "CameraEngineSocketClient.h"

#include "CameraEngineProtocol.h"
#include "SocketLineCodec.h"

#include <QDateTime>
#include <QLocalSocket>
#include <QThread>
#include <QtGlobal>

// 构造套接字客户端
CameraEngineSocketClient::CameraEngineSocketClient()
    : m_socket(new QLocalSocket())
{
}

// 断开连接并释放套接字
CameraEngineSocketClient::~CameraEngineSocketClient()
{
    disconnectFromEngine();
    delete m_socket;
    m_socket = nullptr;
}

// 连接 VisionEngine 控制端
bool CameraEngineSocketClient::connectToEngine(int timeoutMs)
{
    if (!m_socket)
        return false;
    if (m_socket->state() == QLocalSocket::ConnectedState)
        return true;

    m_socket->abort();
    m_socket->connectToServer(CameraEngineProtocol::serverName());
    if (!m_socket->waitForConnected(timeoutMs))
        return false;
    m_readBuffer.clear();
    return true;
}

// 断开与引擎的连接
void CameraEngineSocketClient::disconnectFromEngine()
{
    if (!m_socket)
        return;
    if (m_socket->state() != QLocalSocket::UnconnectedState)
        m_socket->disconnectFromServer();
    m_readBuffer.clear();
}

// 是否已连接引擎
bool CameraEngineSocketClient::isConnected() const
{
    return m_socket && m_socket->state() == QLocalSocket::ConnectedState;
}

// 写入一行协议文本
bool CameraEngineSocketClient::writeLine(const QString &line)
{
    if (!isConnected())
        return false;
    const QByteArray payload = SocketLineCodec::encodeLine(line);
    if (m_socket->write(payload) != payload.size())
        return false;
    return m_socket->waitForBytesWritten(3000);
}

// 发送 ready <frameId>
bool CameraEngineSocketClient::sendReady(quint64 frameId)
{
    return writeLine(CameraEngineProtocol::readyLine(frameId));
}

// 非阻塞读取套接字数据到缓冲
void CameraEngineSocketClient::drainIncoming()
{
    if (!isConnected())
        return;
    if (m_socket->waitForReadyRead(0))
        m_readBuffer += m_socket->readAll();
}

// 阻塞等待引擎回 ack <frameId>
bool CameraEngineSocketClient::waitAck(quint64 expectedFrameId, int timeoutMs)
{
    if (!isConnected())
        return false;

    drainIncoming();

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        QByteArray line;
        while (SocketLineCodec::takeLine(&m_readBuffer, &line))
        {
            quint64 id = 0;
            if (CameraEngineProtocol::parseAck(line, &id) && id == expectedFrameId)
                return true;
        }

        const int remaining = static_cast<int>(deadline - QDateTime::currentMSecsSinceEpoch());
        if (remaining <= 0)
            break;
        if (!m_socket->waitForReadyRead(qMin(remaining, 200)))
            continue;
        m_readBuffer += m_socket->readAll();
    }
    return false;
}

bool CameraEngineSocketClient::sendReadyAndWaitAck(quint64 frameId, int totalTimeoutMs, int attemptTimeoutMs)
{
    if (frameId == 0)
        return false;

    const int attemptMs = qMax(attemptTimeoutMs, 200);
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMax(totalTimeoutMs, attemptMs);

    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        if (!isConnected() && !connectToEngine(3000))
        {
            QThread::msleep(200);
            continue;
        }

        if (!sendReady(frameId))
        {
            disconnectFromEngine();
            QThread::msleep(200);
            continue;
        }

        const int remaining = static_cast<int>(deadline - QDateTime::currentMSecsSinceEpoch());
        if (remaining <= 0)
            break;

        if (waitAck(frameId, qMin(attemptMs, remaining)))
            return true;
    }

    return false;
}
