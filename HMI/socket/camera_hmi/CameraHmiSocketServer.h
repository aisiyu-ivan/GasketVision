#ifndef CAMERAHMISOCKETSERVER_H
#define CAMERAHMISOCKETSERVER_H

#include <QObject>

class QLocalServer;
class QLocalSocket;
class QTimer;

// HMI ← CameraService：监听 GasketVision.CameraHmi.Control
class CameraHmiSocketServer : public QObject
{
    Q_OBJECT

public:
    explicit CameraHmiSocketServer(QObject *parent = nullptr);
    ~CameraHmiSocketServer() override;

    bool listen();
    bool sendAck(quint32 slotIndex, quint64 releasedFrameId);

signals:
    void listenReady();
    void listenFailed(const QString &reason);
    void clientConnected();
    void clientDisconnected();
    void readyReceived(quint64 frameId, quint32 slotIndex);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();
    void checkHeartbeat();

private:
    void parseBufferedLines();

    QLocalServer *m_server = nullptr;
    QLocalSocket *m_client = nullptr;
    QTimer *m_watchdog = nullptr;
    QByteArray m_readBuffer;
    qint64 m_lastSeenMs = 0;
};

#endif // CAMERAHMISOCKETSERVER_H
