#ifndef ENGINEHMISOCKETCLIENT_H
#define ENGINEHMISOCKETCLIENT_H

#include <QByteArray>

class QObject;
class QLocalSocket;

// VisionEngine → HMI：连接 GasketVision.EngineHmi.Control，发送 result、等待 ack
class EngineHmiSocketClient
{
public:
    // 构造套接字客户端
    explicit EngineHmiSocketClient(QObject *socketParent = nullptr);
    // 断开连接并释放套接字
    ~EngineHmiSocketClient();

    EngineHmiSocketClient(const EngineHmiSocketClient &) = delete;
    EngineHmiSocketClient &operator=(const EngineHmiSocketClient &) = delete;

    // 连接 HMI 控制端并发送 hello
    bool connectToHmi(int timeoutMs = 5000);
    // 若未连接则尝试重连
    bool ensureConnected(int timeoutMs = 5000);
    // 断开 HMI 控制连接
    void disconnectFromHmi();
    // 当前是否已连接
    bool isConnected() const;

    // 发送 ready 行：请求 HMI 环缓空位后再写 SHM
    bool sendReady(quint64 frameId, int stationId, bool ok);
    // 发送 result 行（非严格模式可选通知）
    bool sendResult(quint64 frameId, int stationId, bool ok);
    // 发送 result 行（断线重连后重试一次）
    bool sendResultWithRetry(quint64 frameId, int stationId, bool ok);
    // 阻塞等待指定 frameId 的 ack
    bool waitAck(quint64 expectedFrameId, int timeoutMs = 30000);
    // 周期性重发 ready 直至收到 ack 或总超时
    bool sendReadyAndWaitAck(quint64 frameId, int stationId, bool ok, int totalTimeoutMs = 60000,
                             int attemptTimeoutMs = 3000);
    // 发送 ping 保活
    bool sendPing();

private:
    // 向套接字写入一行文本
    bool writeLine(const QString &line);

    QLocalSocket *m_socket = nullptr; // 本地套接字
    QByteArray m_readBuffer; // 接收行缓冲
};

#endif // ENGINEHMISOCKETCLIENT_H
