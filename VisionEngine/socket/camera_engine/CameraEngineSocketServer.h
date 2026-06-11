#ifndef CAMERAENGINESOCKETSERVER_H
#define CAMERAENGINESOCKETSERVER_H

#include <QByteArray>
#include <QtGlobal>

class QLocalServer;
class QLocalSocket;

// VisionEngine ← CameraService：监听 GasketVision.Camera.Control，处理 ready、回 ack
class CameraEngineSocketServer
{
public:
    explicit CameraEngineSocketServer();
    ~CameraEngineSocketServer();

    CameraEngineSocketServer(const CameraEngineSocketServer &) = delete;
    CameraEngineSocketServer &operator=(const CameraEngineSocketServer &) = delete;

    bool listen();
    void close();
    bool isListening() const;
    bool hasClient() const;

    void processReadyRequests(quint64 lastConsumedFrameId);
    bool sendAck(quint64 frameId);

private:
    void acceptPending();
    void drainIncoming();
    void parseIncomingReadyLines();
    static bool hasSlotForFrame(quint64 frameId, quint64 lastConsumedFrameId);
    bool writeLine(const QString &line);

    QLocalServer *m_server = nullptr;
    QLocalSocket *m_client = nullptr;
    QByteArray m_readBuffer;
    quint64 m_pendingReadyFrameId = 0;
};

#endif // CAMERAENGINESOCKETSERVER_H
