#ifndef HMISTATUSSOCKETCLIENT_H
#define HMISTATUSSOCKETCLIENT_H

#include <QObject>

class QLocalSocket;
class QTimer;

// 引擎 → HMI：本地套接字上报在线状态（hello / ping），不传检测数据
class HmiStatusSocketClient : public QObject
{
    Q_OBJECT

public:
    // 构造状态套接字客户端
    explicit HmiStatusSocketClient(QObject *parent = nullptr);

    // 连接 HMI 状态套接字并启动定时 ping
    bool connectToServer(int timeoutMs = 5000);
    // 断开连接并停止心跳
    void disconnectFromServer();
    // 当前是否已连接
    bool isConnected() const;

private:
    QLocalSocket *m_socket = nullptr; // 本地套接字
    QTimer *m_heartbeatTimer = nullptr; // 心跳定时器
};

#endif // HMISTATUSSOCKETCLIENT_H
