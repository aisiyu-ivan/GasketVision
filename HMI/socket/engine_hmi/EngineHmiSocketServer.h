#ifndef ENGINEHMISOCKETSERVER_H
#define ENGINEHMISOCKETSERVER_H

#include <QObject>

class QLocalServer;
class QLocalSocket;
class QTimer;

// HMI ← VisionEngine：监听 GasketVision.EngineHmi.Control，接收 result/ping，向 Engine 发送 ack
class EngineHmiSocketServer : public QObject
{
    Q_OBJECT

public:
    // 构造 Engine-HMI 本地套接字服务端
    explicit EngineHmiSocketServer(QObject *parent = nullptr);
    // 释放监听与客户端连接
    ~EngineHmiSocketServer() override;

    // 在 GasketVision.EngineHmi.Control 上开始监听
    bool listen();

    // HMI 已完成 record，通知 VisionEngine 可继续下一帧
    bool sendAck(quint64 frameId);

public slots:
    // 同线程发送 ack（通信线程内 readFrame 后调用）
    bool deliverAck(quint64 frameId);

signals:
    // 本地套接字 listen 成功
    void listenReady();
    // 本地套接字 listen 失败
    void listenFailed(const QString &reason);
    // VisionEngine 已连接
    void clientConnected();
    // VisionEngine 已断开
    void clientDisconnected();
    // 收到 Engine ready：请求写入 HMI SHM（环缓反压）
    void readyReceived(quint64 frameId, int stationId, bool ok);
    // 收到 result 行（非严格模式可选）
    void resultReceived(quint64 frameId, int stationId, bool ok);

private slots:
    // 接受 VisionEngine 连接并启动心跳
    void onNewConnection();
    // 读取套接字数据并解析行协议
    void onReadyRead();
    // 客户端断开时清理连接
    void onDisconnected();
    // 心跳超时则强制断开
    void checkHeartbeat();

private:
    // 从读缓冲中逐行解析 result 命令
    void parseBufferedLines();

    QLocalServer *m_server = nullptr;   // 本地套接字服务端
    QLocalSocket *m_client = nullptr;   // 当前连接的 VisionEngine 客户端
    QTimer *m_watchdog = nullptr;       // 心跳超时检测定时器
    QByteArray m_readBuffer;            // 套接字读缓冲（行协议）
    qint64 m_lastSeenMs = 0;            // 最近一次收到数据的时间戳
};

#endif // ENGINEHMISOCKETSERVER_H
