#ifndef HMIIPCLAYOUT_H
#define HMIIPCLAYOUT_H

#include <QtGlobal>
#include <QString>

// 引擎 ↔ HMI：环形缓冲 SHM 布局与跨进程对象命名
namespace HmiIpc
{
constexpr quint32 kMagic = 0x47564950u; // 'GVIP'
constexpr quint32 kVersion = 1;
constexpr int kRingSlots = 4;
constexpr int kMaxImageBytes = 1920 * 1080 * 3;

// 像素格式
enum ImageFormat : quint8
{
    Gray8 = 0,
    Bgr888 = 1,
};

// 缺陷类型编码
enum DefectCode : quint8
{
    DefectNone = 0,
    DefectMissing = 1,
    DefectSize = 2,
    DefectEccentric = 3,
    DefectUnknown = 255,
};

// 共享内存区域键名
inline QString shmKey()
{
    return QStringLiteral("GasketVision.Ipc.Region");
}

// 读写互斥 Mutex 名
inline QString mutexName()
{
    return QStringLiteral("GasketVision.Ipc.Mutex");
}

// 新帧计数信号量名
inline QString frameSemName()
{
    return QStringLiteral("GasketVision.Ipc.FrameReady");
}

#pragma pack(push, 1)
// 单张图像平面头（原图或标注图）
struct ImagePlaneHeader
{
    quint32 magic = 0; // 魔数
    quint64 frameId = 0; // 帧序号
    quint32 width = 0; // 图像宽度（像素）
    quint32 height = 0; // 图像高度（像素）
    quint32 bytesPerLine = 0; // 每行字节数
    quint8 format = 0; // 像素格式（ImageFormat）
    quint8 reserved[3] = {}; // 保留
    quint32 payloadSize = 0; // 像素 payload 字节数
};

// 区域首部控制块（判定结果 + 测量值 + 路径）
struct ControlBlock
{
    quint32 magic = 0; // 魔数
    quint32 version = 0; // 协议版本
    quint64 frameId = 0; // 帧序号
    qint64 timestampMs = 0; // 时间戳（毫秒）
    quint32 slotIndex = 0; // 环槽索引
    quint32 stationId = 0; // 工位编号
    quint8 ok = 0; // 是否 OK（1/0）
    quint8 defectCode = 0; // 缺陷编码（DefectCode）
    quint8 flags = 0; // bit0 original, bit1 annotated
    quint8 reserved = 0; // 保留
    double outerDiameterMm = 0.0; // 外径（mm）
    double innerDiameterMm = 0.0; // 内径（mm）
    double offsetXMm = 0.0; // X 偏心（mm）
    double offsetYMm = 0.0; // Y 偏心（mm）
    double matchScore = 0.0; // 模板匹配得分
    char defectText[64] = {}; // 缺陷中文描述
    char cameraStatus[64] = {}; // 相机状态文本
    char imagePath[260] = {}; // 原图路径
    char annotatedPath[260] = {}; // 标注图路径
};
#pragma pack(pop)

constexpr quint8 kFlagHasOriginal = 0x01;
constexpr quint8 kFlagHasAnnotated = 0x02;

// 单张图平面占用字节（头 + 最大像素）
inline qsizetype imagePlaneStorageBytes()
{
    return static_cast<qsizetype>(sizeof(ImagePlaneHeader)) + kMaxImageBytes;
}

// 单环槽字节（原图平面 + 标注图平面）
inline qsizetype ringSlotBytes()
{
    return imagePlaneStorageBytes() * 2;
}

// 整段 SHM 区域字节数
inline qsizetype regionBytes()
{
    return static_cast<qsizetype>(sizeof(ControlBlock)) + ringSlotBytes() * kRingSlots;
}

// SHM 映射基地址
inline char *regionBase(void *shm)
{
    return static_cast<char *>(shm);
}

// 区域首部控制块指针
inline ControlBlock *controlBlock(void *shm)
{
    return reinterpret_cast<ControlBlock *>(regionBase(shm));
}

// 指定环槽起始地址
inline char *slotBase(void *shm, quint32 slotIndex)
{
    return regionBase(shm) + sizeof(ControlBlock) + ringSlotBytes() * slotIndex;
}

// 环槽内原图平面头指针
inline ImagePlaneHeader *originalHeader(void *shm, quint32 slotIndex)
{
    return reinterpret_cast<ImagePlaneHeader *>(slotBase(shm, slotIndex));
}

// 环槽内原图像素 payload 指针
inline uchar *originalPayload(void *shm, quint32 slotIndex)
{
    return reinterpret_cast<uchar *>(originalHeader(shm, slotIndex) + 1);
}

// 环槽内标注图平面头指针
inline ImagePlaneHeader *annotatedHeader(void *shm, quint32 slotIndex)
{
    return reinterpret_cast<ImagePlaneHeader *>(slotBase(shm, slotIndex) + imagePlaneStorageBytes());
}

// 环槽内标注图像素 payload 指针
inline uchar *annotatedPayload(void *shm, quint32 slotIndex)
{
    return reinterpret_cast<uchar *>(annotatedHeader(shm, slotIndex) + 1);
}

} // namespace HmiIpc

#endif // HMIIPCLAYOUT_H
