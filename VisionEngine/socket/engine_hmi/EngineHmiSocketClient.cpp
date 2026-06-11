#include "EngineHmiSocketClient.h"

#include "EngineHmiProtocol.h"
#include "SocketLineCodec.h"

#include <QDateTime>
#include <QLocalSocket>
#include <QThread>
#include <QtGlobal>

// 构造套接字客户端
EngineHmiSocketClient::EngineHmiSocketClient(QObject *socketParent)
    : m_socket(new QLocalSocket(socketParent))
{
}

// 断开连接并释放套接字
EngineHmiSocketClient::~EngineHmiSocketClient()
{
    disconnectFromHmi();
    if (m_socket && !m_socket->parent())
        delete m_socket;
    m_socket = nullptr;
}

// 连接 HMI 控制端并发送 hello
bool EngineHmiSocketClient::connectToHmi(int timeoutMs)
{
    if (!m_socket)
        return false;
    if (m_socket->state() == QLocalSocket::ConnectedState)
        return true;

    m_socket->abort();
    m_socket->connectToServer(EngineHmiProtocol::serverName());
    if (!m_socket->waitForConnected(timeoutMs))
        return false;
    m_readBuffer.clear();
    return writeLine(EngineHmiProtocol::helloLine());
}

// 若未连接则尝试重连
bool EngineHmiSocketClient::ensureConnected(int timeoutMs)
{
    if (isConnected())
        return true;
    return connectToHmi(timeoutMs);
}

// 断开 HMI 控制连接
void EngineHmiSocketClient::disconnectFromHmi()
{
    if (!m_socket)
        return;
    if (m_socket->state() != QLocalSocket::UnconnectedState)
        m_socket->disconnectFromServer();
    m_readBuffer.clear();
}

// 当前是否已连接
bool EngineHmiSocketClient::isConnected() const
{
    return m_socket && m_socket->state() == QLocalSocket::ConnectedState;
}

// 向套接字写入一行文本
bool EngineHmiSocketClient::writeLine(const QString &line)
{
    if (!isConnected())
        return false;
    const QByteArray payload = SocketLineCodec::encodeLine(line);
    if (m_socket->write(payload) != payload.size())
        return false;
    return m_socket->waitForBytesWritten(3000);
}

// 发送 ready 行：请求 HMI 环缓空位
bool EngineHmiSocketClient::sendReady(quint64 frameId, int stationId, bool ok)
{
    return writeLine(EngineHmiProtocol::readyLine(frameId, stationId, ok));
}

// 发送 result 行（单次）
bool EngineHmiSocketClient::sendResult(quint64 frameId, int stationId, bool ok)
{
    return writeLine(EngineHmiProtocol::resultLine(frameId, stationId, ok));
}

// 发送 result 行（断线重连后重试一次）
bool EngineHmiSocketClient::sendResultWithRetry(quint64 frameId, int stationId, bool ok)
{
    if (!ensureConnected())
        return false;
    if (sendResult(frameId, stationId, ok))
        return true;
    if (!ensureConnected())
        return false;
    return sendResult(frameId, stationId, ok);
}

// 发送 ping 保活
bool EngineHmiSocketClient::sendPing()
{
    return writeLine(EngineHmiProtocol::pingLine());
}

// 阻塞等待指定 frameId 的 ack
bool EngineHmiSocketClient::waitAck(quint64 expectedFrameId, int timeoutMs)
{
    if (!isConnected())
        return false;

    if (m_socket->bytesAvailable() > 0)
        m_readBuffer += m_socket->readAll();

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    qint64 lastPingMs = QDateTime::currentMSecsSinceEpoch();
    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - lastPingMs >= 1000)
        {
            sendPing();
            lastPingMs = now;
        }

        QByteArray line;
        while (SocketLineCodec::takeLine(&m_readBuffer, &line))
        {
            quint64 id = 0;
            if (EngineHmiProtocol::parseAck(line, &id) && id == expectedFrameId)
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

bool EngineHmiSocketClient::sendReadyAndWaitAck(quint64 frameId, int stationId, bool ok, int totalTimeoutMs,
                                                int attemptTimeoutMs)
{
    if (frameId == 0)
        return false;

    const int attemptMs = qMax(attemptTimeoutMs, 200);
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMax(totalTimeoutMs, attemptMs);

    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        if (!ensureConnected(3000))
        {
            QThread::msleep(200);
            continue;
        }

        if (!sendReady(frameId, stationId, ok))
        {
            disconnectFromHmi();
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
