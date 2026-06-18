#ifndef CAMERAHMISOCKETCLIENT_H
#define CAMERAHMISOCKETCLIENT_H

#include <QByteArray>

class QLocalSocket;

// CameraService → HMI：连接 GasketVision.CameraHmi.Control，ready/ack 槽位覆写
class CameraHmiSocketClient
{
public:
    CameraHmiSocketClient();
    ~CameraHmiSocketClient();

    CameraHmiSocketClient(const CameraHmiSocketClient &) = delete;
    CameraHmiSocketClient &operator=(const CameraHmiSocketClient &) = delete;

    bool connectToHmi(int timeoutMs = 5000);
    void disconnectFromHmi();
    bool isConnected() const;

    bool sendReady(quint64 frameId, quint32 slotIndex);
    bool sendPing();
    bool sendReadyAndWaitAck(quint64 frameId, quint32 slotIndex, int totalTimeoutMs = 60000,
                             int attemptTimeoutMs = 3000);

    quint64 lastAckedFrameId() const { return m_lastAckedFrameId; }

private:
    bool writeLine(const QString &line);
    void drainIncoming();
    bool waitAck(quint32 expectedSlotIndex, int timeoutMs);

    QLocalSocket *m_socket = nullptr;
    QByteArray m_readBuffer;
    quint64 m_lastAckedFrameId = 0;
};

#endif // CAMERAHMISOCKETCLIENT_H
