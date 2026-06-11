#ifndef HMISTATUSSOCKETSERVER_H
#define HMISTATUSSOCKETSERVER_H

#include <QObject>

class QLocalServer;
class QLocalSocket;
class QTimer;

// HMI ← 引擎：本地套接字接收在线状态与心跳，不传检测帧
class HmiStatusSocketServer : public QObject
{
    Q_OBJECT

public:
    // 构造状态套接字服务端
    explicit HmiStatusSocketServer(QObject *parent = nullptr);
    // 停止监听并释放客户端连接
    ~HmiStatusSocketServer() override;

    // 在 GasketVision.Control 上开始监听
    bool listen();

signals:
    // listen 成功
    void listenReady();
    // VisionEngine 已连接
    void clientConnected();
    // VisionEngine 断开或心跳超时
    void clientDisconnected();
    // listen 失败
    void listenFailed(const QString &reason);

private slots:
    // 接受 VisionEngine 连接
    void onNewConnection();
    // 读取 hello/ping 并刷新心跳时间
    void onReadyRead();
    // 客户端断开
    void onDisconnected();
    // 心跳超时则强制断开
    void checkHeartbeat();

private:
    QLocalServer *m_server = nullptr; // 本地套接字服务端
    QLocalSocket *m_client = nullptr; // 当前连接的 VisionEngine 客户端
    QTimer *m_watchdog = nullptr;     // 心跳超时检测定时器
    qint64 m_lastSeenMs = 0;          // 最近一次收到数据的时间戳
};

#endif // HMISTATUSSOCKETSERVER_H
