#ifndef CAMERAHMIPROTOCOL_H
#define CAMERAHMIPROTOCOL_H

#include <QByteArray>
#include <QList>
#include <QString>

// CameraService ↔ HMI 本地套接字（端点 GasketVision.CameraHmi.Control）
namespace CameraHmiProtocol
{
inline QString serverName()
{
    return QStringLiteral("GasketVision.CameraHmi.Control");
}

inline QString helloLine()
{
    return QStringLiteral("hello");
}

inline QString pingLine()
{
    return QStringLiteral("ping");
}

// Camera → HMI：申请覆写 slotIndex 槽（frameId 为拟写入序号）
inline QString readyLine(quint64 frameId, quint32 slotIndex)
{
    return QStringLiteral("ready %1 %2").arg(frameId).arg(slotIndex);
}

// HMI → Camera：slotIndex 槽已展示+落盘，可覆写；releasedFrameId 为刚释放的帧
inline QString ackLine(quint32 slotIndex, quint64 releasedFrameId)
{
    return QStringLiteral("ack %1 %2").arg(slotIndex).arg(releasedFrameId);
}

inline bool parseReady(const QByteArray &line, quint64 *frameIdOut, quint32 *slotIndexOut)
{
    if (!frameIdOut || !slotIndexOut || !line.startsWith("ready "))
        return false;
    const QList<QByteArray> parts = line.mid(6).split(' ');
    if (parts.size() < 2)
        return false;
    bool ok = false;
    const quint64 frameId = parts.at(0).toULongLong(&ok);
    if (!ok)
        return false;
    const quint32 slot = static_cast<quint32>(parts.at(1).toUInt(&ok));
    if (!ok)
        return false;
    *frameIdOut = frameId;
    *slotIndexOut = slot;
    return true;
}

inline bool parseAck(const QByteArray &line, quint32 *slotIndexOut, quint64 *releasedFrameIdOut)
{
    if (!slotIndexOut || !releasedFrameIdOut || !line.startsWith("ack "))
        return false;
    const QList<QByteArray> parts = line.mid(4).split(' ');
    if (parts.size() < 2)
        return false;
    bool ok = false;
    const quint32 slot = static_cast<quint32>(parts.at(0).toUInt(&ok));
    if (!ok)
        return false;
    const quint64 releasedId = parts.at(1).toULongLong(&ok);
    if (!ok)
        return false;
    *slotIndexOut = slot;
    *releasedFrameIdOut = releasedId;
    return true;
}

} // namespace CameraHmiProtocol

#endif // CAMERAHMIPROTOCOL_H
