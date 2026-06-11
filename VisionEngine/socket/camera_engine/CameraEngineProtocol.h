#ifndef CAMERAENGINEPROTOCOL_H
#define CAMERAENGINEPROTOCOL_H

#include <QByteArray>
#include <QString>

// CameraService ↔ VisionEngine 本地套接字协议（端点 GasketVision.Camera.Control）
namespace CameraEngineProtocol
{
// 本地套接字服务端名称
inline QString serverName()
{
    return QStringLiteral("GasketVision.Camera.Control");
}

// Camera → Engine：请求写入下一帧（frameId 为拟发布序号）
inline QString readyLine(quint64 frameId)
{
    return QStringLiteral("ready %1").arg(frameId);
}

// Engine → Camera：环缓有空位，允许按 frameId 采图并写入 SHM
inline QString ackLine(quint64 frameId)
{
    return QStringLiteral("ack %1").arg(frameId);
}

// 解析 ready 行，提取 frameId
inline bool parseReady(const QByteArray &line, quint64 *frameIdOut)
{
    if (!frameIdOut || !line.startsWith("ready "))
        return false;
    bool ok = false;
    const quint64 id = line.mid(6).toULongLong(&ok);
    if (!ok)
        return false;
    *frameIdOut = id;
    return true;
}

// 解析 ack 行，提取 frameId
inline bool parseAck(const QByteArray &line, quint64 *frameIdOut)
{
    if (!frameIdOut || !line.startsWith("ack "))
        return false;
    bool ok = false;
    const quint64 id = line.mid(4).toULongLong(&ok);
    if (!ok)
        return false;
    *frameIdOut = id;
    return true;
}

} // namespace CameraEngineProtocol

#endif // CAMERAENGINEPROTOCOL_H
