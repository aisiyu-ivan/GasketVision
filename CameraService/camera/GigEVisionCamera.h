#ifndef GIGEVISIONCAMERA_H
#define GIGEVISIONCAMERA_H

#include "IVisionImageSource.h"

#include <QAbstractSocket>
#include <QTimer>

class QTcpSocket;

// GigE 相机连接状态
enum class GigECameraState
{
    Disconnected, // 未连接
    Connecting,   // 连接中
    Grabbing,     // 已连接、可采图
    Error         // 连接或采图错误
};

// GigE 工业相机采图源（骨架实现，待接入 SDK）
class GigEVisionCamera : public IVisionImageSource
{
public:
    // 读取相机 IP/端口配置并尝试连接
    bool open(const QJsonObject &config) override;
    // 断开相机并清理 socket 与定时器
    void close() override;
    // 是否处于采图就绪状态
    bool isConnected() const override;
    // 返回相机连接状态的中文描述
    QString statusText() const override;
    // 从相机抓取一帧（当前为占位，返回 false）
    bool grabFrame(VisionFrame &out) override;

private:
    // 更新内部状态枚举
    void setState(GigECameraState state);
    // 断线后尝试重新连接
    void tryReconnect();

    QJsonObject m_config;                        // 完整配置 JSON
    QTcpSocket *m_socket = nullptr;              // 相机 TCP 套接字
    QTimer *m_reconnectTimer = nullptr;          // 断线重连定时器
    GigECameraState m_state = GigECameraState::Disconnected; // 当前连接状态
    QString m_host;                              // 相机 IP 地址
    quint16 m_port = 3956;                       // GigE Vision 控制端口
    int m_reconnectMs = 3000;                    // 重连间隔（毫秒）
};

#endif // GIGEVISIONCAMERA_H
