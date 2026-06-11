#include "CameraEngineSocketServer.h"

#include "CameraEngineProtocol.h"
#include "CameraIpcLayout.h"
#include "SocketLineCodec.h"

#include <QLocalServer>
#include <QLocalSocket>

CameraEngineSocketServer::CameraEngineSocketServer()
    : m_server(new QLocalServer())
{
}

CameraEngineSocketServer::~CameraEngineSocketServer()
{
    close();
    delete m_server;
    m_server = nullptr;
}

bool CameraEngineSocketServer::listen()
{
    if (!m_server)
        return false;
    QLocalServer::removeServer(CameraEngineProtocol::serverName());
    if (!m_server->listen(CameraEngineProtocol::serverName()))
        return false;
    acceptPending();
    return true;
}

void CameraEngineSocketServer::close()
{
    if (m_client)
    {
        m_client->disconnectFromServer();
        delete m_client;
        m_client = nullptr;
    }
    if (m_server && m_server->isListening())
        m_server->close();
    m_readBuffer.clear();
    m_pendingReadyFrameId = 0;
}

bool CameraEngineSocketServer::isListening() const
{
    return m_server && m_server->isListening();
}

bool CameraEngineSocketServer::hasClient() const
{
    return m_client && m_client->state() == QLocalSocket::ConnectedState;
}

void CameraEngineSocketServer::acceptPending()
{
    if (!m_server)
        return;
    while (m_server->hasPendingConnections())
    {
        if (m_client)
        {
            m_client->disconnectFromServer();
            delete m_client;
            m_client = nullptr;
        }
        m_client = m_server->nextPendingConnection();
    }
}

void CameraEngineSocketServer::drainIncoming()
{
    if (!hasClient())
        acceptPending();
    if (!hasClient())
        return;
    while (m_client->bytesAvailable() > 0)
        m_readBuffer += m_client->readAll();
}

void CameraEngineSocketServer::parseIncomingReadyLines()
{
    QByteArray line;
    while (SocketLineCodec::takeLine(&m_readBuffer, &line))
    {
        quint64 frameId = 0;
        if (CameraEngineProtocol::parseReady(line, &frameId))
            m_pendingReadyFrameId = frameId;
    }
}

bool CameraEngineSocketServer::hasSlotForFrame(quint64 frameId, quint64 lastConsumedFrameId)
{
    if (frameId == 0)
        return false;
    return frameId <= lastConsumedFrameId + static_cast<quint64>(CameraIpc::kRingSlots);
}

void CameraEngineSocketServer::processReadyRequests(quint64 lastConsumedFrameId)
{
    acceptPending();
    drainIncoming();
    parseIncomingReadyLines();

    if (m_pendingReadyFrameId == 0)
        return;
    if (!hasSlotForFrame(m_pendingReadyFrameId, lastConsumedFrameId))
        return;

    const quint64 ackId = m_pendingReadyFrameId;
    m_pendingReadyFrameId = 0;
    sendAck(ackId);
}

bool CameraEngineSocketServer::writeLine(const QString &line)
{
    if (!hasClient())
        acceptPending();
    if (!hasClient())
        return false;
    const QByteArray payload = SocketLineCodec::encodeLine(line);
    if (m_client->write(payload) != payload.size())
        return false;
    return m_client->waitForBytesWritten(3000);
}

bool CameraEngineSocketServer::sendAck(quint64 frameId)
{
    return writeLine(CameraEngineProtocol::ackLine(frameId));
}
