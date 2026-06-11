#ifndef ENGINEHMIPROTOCOL_H
#define ENGINEHMIPROTOCOL_H

#include <QByteArray>
#include <QList>
#include <QString>

// VisionEngine ↔ HMI 本地套接字协议（端点 GasketVision.EngineHmi.Control）
namespace EngineHmiProtocol
{
// 本地套接字服务端名称
inline QString serverName()
{
    return QStringLiteral("GasketVision.EngineHmi.Control");
}

// 连接握手首行
inline QString helloLine()
{
    return QStringLiteral("hello");
}

// 心跳保活行
inline QString pingLine()
{
    return QStringLiteral("ping");
}

// Engine → HMI：请求写入下一帧检测结果（frameId 为拟发布序号）
inline QString readyLine(quint64 frameId, int stationId, bool ok)
{
    return QStringLiteral("ready %1 %2 %3").arg(frameId).arg(stationId).arg(ok ? 1 : 0);
}

// Engine → HMI：SHM 已写入（非严格模式可选通知）
inline QString resultLine(quint64 frameId, int stationId, bool ok)
{
    return QStringLiteral("result %1 %2 %3").arg(frameId).arg(stationId).arg(ok ? 1 : 0);
}

// HMI → Engine：环缓有空位，允许按 frameId 写入 SHM
inline QString ackLine(quint64 frameId)
{
    return QStringLiteral("ack %1").arg(frameId);
}

// 解析 ready 行，提取 frameId、工位与 OK/NG
inline bool parseReady(const QByteArray &line, quint64 *frameIdOut, int *stationIdOut, bool *okOut)
{
    if (!frameIdOut || !stationIdOut || !okOut || !line.startsWith("ready "))
        return false;
    const QList<QByteArray> parts = line.mid(6).split(' ');
    if (parts.size() < 3)
        return false;
    bool okParse = false;
    const quint64 frameId = parts.at(0).toULongLong(&okParse);
    if (!okParse)
        return false;
    const int stationId = parts.at(1).toInt(&okParse);
    if (!okParse)
        return false;
    const int okVal = parts.at(2).toInt(&okParse);
    if (!okParse)
        return false;
    *frameIdOut = frameId;
    *stationIdOut = stationId;
    *okOut = okVal != 0;
    return true;
}

// 解析 result 行，提取 frameId、工位与 OK/NG
inline bool parseResult(const QByteArray &line, quint64 *frameIdOut, int *stationIdOut, bool *okOut)
{
    if (!frameIdOut || !stationIdOut || !okOut || !line.startsWith("result "))
        return false;
    const QList<QByteArray> parts = line.mid(7).split(' ');
    if (parts.size() < 3)
        return false;
    bool okParse = false;
    const quint64 frameId = parts.at(0).toULongLong(&okParse);
    if (!okParse)
        return false;
    const int stationId = parts.at(1).toInt(&okParse);
    if (!okParse)
        return false;
    const int okVal = parts.at(2).toInt(&okParse);
    if (!okParse)
        return false;
    *frameIdOut = frameId;
    *stationIdOut = stationId;
    *okOut = okVal != 0;
    return true;
}

// 解析 ack 行，提取已确认的 frameId
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

} // namespace EngineHmiProtocol

#endif // ENGINEHMIPROTOCOL_H
