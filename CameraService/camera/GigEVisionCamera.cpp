#include "GigEVisionCamera.h"

#include <QTcpSocket>

// 读取相机配置并尝试 TCP 连接（骨架）
bool GigEVisionCamera::open(const QJsonObject &config)
{
    close();
    m_config = config;
    const QJsonObject cam = config.value(QStringLiteral("camera")).toObject();
    m_host = cam.value(QStringLiteral("ip")).toString(QStringLiteral("192.168.1.100"));
    m_port = static_cast<quint16>(cam.value(QStringLiteral("port")).toInt(3956));
    m_reconnectMs = cam.value(QStringLiteral("reconnectMs")).toInt(3000);

    m_socket = new QTcpSocket();
    m_reconnectTimer = new QTimer();
    m_reconnectTimer->setInterval(m_reconnectMs);

    setState(GigECameraState::Connecting);
    m_socket->connectToHost(m_host, m_port);
    if (m_socket->waitForConnected(2000))
        setState(GigECameraState::Grabbing);
    else
        setState(GigECameraState::Error);

    return m_state == GigECameraState::Grabbing;
}

// 停止定时器、断开 socket 并重置状态
void GigEVisionCamera::close()
{
    if (m_reconnectTimer)
    {
        m_reconnectTimer->stop();
        delete m_reconnectTimer;
        m_reconnectTimer = nullptr;
    }
    if (m_socket)
    {
        m_socket->disconnectFromHost();
        delete m_socket;
        m_socket = nullptr;
    }
    setState(GigECameraState::Disconnected);
}

// 是否处于 Grabbing 就绪状态
bool GigEVisionCamera::isConnected() const
{
    return m_state == GigECameraState::Grabbing;
}

// 根据当前状态返回中文描述
QString GigEVisionCamera::statusText() const
{
    switch (m_state)
    {
    case GigECameraState::Disconnected:
        return QStringLiteral("断开");
    case GigECameraState::Connecting:
        return QStringLiteral("连接中");
    case GigECameraState::Grabbing:
        return QStringLiteral("已连接");
    case GigECameraState::Error:
        return QStringLiteral("相机错误（请切 synthetic 模式）");
    }
    return QStringLiteral("未知");
}

// 抓取相机帧（占位，待接入 SDK）
bool GigEVisionCamera::grabFrame(VisionFrame &out)
{
    Q_UNUSED(out);
    // SDK hook: receive frame buffer from GigE camera driver (MVS/Halcon/etc.)
    return false;
}

// 更新内部连接状态
void GigEVisionCamera::setState(GigECameraState state)
{
    m_state = state;
}

// 断线后重新发起 TCP 连接
void GigEVisionCamera::tryReconnect()
{
    if (!m_socket)
        return;
    setState(GigECameraState::Connecting);
    m_socket->connectToHost(m_host, m_port);
}
