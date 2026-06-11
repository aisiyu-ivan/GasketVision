#ifndef CAMERAENGINESOCKETCLIENT_H
#define CAMERAENGINESOCKETCLIENT_H

#include <QByteArray>

class QLocalSocket;

// CameraService → VisionEngine：连接 GasketVision.Camera.Control，发送 ready、等待 ack
class CameraEngineSocketClient
{
public:
    // 构造套接字客户端
    CameraEngineSocketClient();
    // 断开连接并释放套接字
    ~CameraEngineSocketClient();

    CameraEngineSocketClient(const CameraEngineSocketClient &) = delete;
    CameraEngineSocketClient &operator=(const CameraEngineSocketClient &) = delete;

    // 连接 VisionEngine 控制端（GasketVision.Camera.Control）
    bool connectToEngine(int timeoutMs = 5000);
    // 断开与引擎的连接
    void disconnectFromEngine();
    // 是否已连接引擎
    bool isConnected() const;

    // 发送 ready <frameId>，请求 Engine 环缓空位后再采图写 SHM
    bool sendReady(quint64 frameId);
    // 阻塞等待 Engine 回 ack <frameId>（允许传图）
    bool waitAck(quint64 expectedFrameId, int timeoutMs = 30000);
    // 周期性重发 ready 直至收到 ack 或总超时
    bool sendReadyAndWaitAck(quint64 frameId, int totalTimeoutMs = 60000, int attemptTimeoutMs = 3000);

private:
    // 写入一行协议文本
    bool writeLine(const QString &line);
    // 非阻塞读取套接字数据到缓冲
    void drainIncoming();

    QLocalSocket *m_socket = nullptr; // 本地套接字
    QByteArray m_readBuffer;          // 接收行缓冲
};

#endif // CAMERAENGINESOCKETCLIENT_H
