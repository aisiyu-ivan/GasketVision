#include "CameraHmiSocketClient.h"

#include "CameraHmiProtocol.h"
#include "SocketLineCodec.h"

#include <QDateTime>
#include <QLocalSocket>
#include <QThread>
#include <QtGlobal>

CameraHmiSocketClient::CameraHmiSocketClient()
    : m_socket(new QLocalSocket())
{
}

CameraHmiSocketClient::~CameraHmiSocketClient()
{
    disconnectFromHmi();
    delete m_socket;
    m_socket = nullptr;
}

bool CameraHmiSocketClient::connectToHmi(int timeoutMs)
{
    if (!m_socket)
        return false;
    if (m_socket->state() == QLocalSocket::ConnectedState)
        return true;

    m_socket->abort();
    m_socket->connectToServer(CameraHmiProtocol::serverName());
    if (!m_socket->waitForConnected(timeoutMs))
        return false;
    m_readBuffer.clear();
    return writeLine(CameraHmiProtocol::helloLine());
}

void CameraHmiSocketClient::disconnectFromHmi()
{
    if (!m_socket)
        return;
    if (m_socket->state() != QLocalSocket::UnconnectedState)
        m_socket->disconnectFromServer();
    m_readBuffer.clear();
}

bool CameraHmiSocketClient::isConnected() const
{
    return m_socket && m_socket->state() == QLocalSocket::ConnectedState;
}

bool CameraHmiSocketClient::writeLine(const QString &line)
{
    if (!isConnected())
        return false;
    const QByteArray payload = SocketLineCodec::encodeLine(line);
    if (m_socket->write(payload) != payload.size())
        return false;
    return m_socket->waitForBytesWritten(3000);
}

bool CameraHmiSocketClient::sendReady(quint64 frameId, quint32 slotIndex)
{
    return writeLine(CameraHmiProtocol::readyLine(frameId, slotIndex));
}

bool CameraHmiSocketClient::sendPing()
{
    return writeLine(CameraHmiProtocol::pingLine());
}

void CameraHmiSocketClient::drainIncoming()
{
    if (!isConnected())
        return;
    if (m_socket->bytesAvailable() > 0)
        m_readBuffer += m_socket->readAll();
}

bool CameraHmiSocketClient::waitAck(quint32 expectedSlotIndex, int timeoutMs)
{
    if (!isConnected())
        return false;

    drainIncoming();

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
            quint32 slot = 0;
            quint64 releasedId = 0;
            if (CameraHmiProtocol::parseAck(line, &slot, &releasedId) && slot == expectedSlotIndex)
            {
                if (releasedId > m_lastAckedFrameId)
                    m_lastAckedFrameId = releasedId;
                return true;
            }
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

bool CameraHmiSocketClient::sendReadyAndWaitAck(quint64 frameId, quint32 slotIndex, int totalTimeoutMs,
                                                int attemptTimeoutMs)
{
    if (frameId == 0)
        return false;

    const int attemptMs = qMax(attemptTimeoutMs, 200);
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMax(totalTimeoutMs, attemptMs);

    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        if (!isConnected() && !connectToHmi(3000))
        {
            QThread::msleep(200);
            continue;
        }

        if (!sendReady(frameId, slotIndex))
        {
            disconnectFromHmi();
            QThread::msleep(200);
            continue;
        }

        const int remaining = static_cast<int>(deadline - QDateTime::currentMSecsSinceEpoch());
        if (remaining <= 0)
            break;

        if (waitAck(slotIndex, qMin(attemptMs, remaining)))
            return true;
    }

    return false;
}
