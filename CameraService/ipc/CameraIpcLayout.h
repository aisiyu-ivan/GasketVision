#ifndef CAMERALIPCLAYOUT_H
#define CAMERALIPCLAYOUT_H

#include <QtGlobal>

// 相机进程 → 检测引擎：环形 SHM 布局与跨进程对象命名
namespace CameraIpc
{
constexpr quint32 kMagic = 0x47564349u; // 'GVCI'
constexpr quint32 kVersion = 2;           // 布局版本号
constexpr int kRingSlots = 4;             // 环形缓冲槽位数
constexpr int kMaxImageBytes = 1920 * 1080 * 3; // 单槽最大像素字节数

// 像素格式
enum ImageFormat : quint8
{
    Gray8 = 0,   // 单通道灰度
    Bgr888 = 1,  // 三通道 BGR
};

// 共享内存区域键名
inline QString shmKey()
{
    return QStringLiteral("GasketVision.Camera.Ipc.Region");
}

// SHM 读写互斥 Mutex 名
inline QString mutexName()
{
    return QStringLiteral("GasketVision.Camera.Ipc.Mutex");
}

// 新帧计数信号量名
inline QString frameSemName()
{
    return QStringLiteral("GasketVision.Camera.Ipc.FrameReady");
}

#pragma pack(push, 1)
// 环槽内图像平面头（紧跟像素 payload）
struct ImagePlaneHeader
{
    quint32 magic = 0;        // 魔数 GVCI
    quint64 frameId = 0;      // 帧序号
    quint32 width = 0;        // 图像宽度（像素）
    quint32 height = 0;       // 图像高度（像素）
    quint32 bytesPerLine = 0; // 每行字节数
    quint8 format = 0;        // ImageFormat 编码
    quint8 reserved[3] = {};
    quint32 payloadSize = 0;  // 像素区实际字节数
};

// 区域首部控制块（指向当前最新帧）
struct ControlBlock
{
    quint32 magic = 0;         // 魔数 GVCI
    quint32 version = 0;       // 布局版本
    quint64 frameId = 0;       // 当前最新帧序号
    qint64 timestampMs = 0;    // 采图时间戳（毫秒）
    quint32 slotIndex = 0;     // 环槽下标
    char sourcePath[260] = {}; // 样本/源文件路径（UTF-8）
    char cameraStatus[64] = {}; // 相机状态文案（UTF-8）
};
#pragma pack(pop)

// 单个环槽字节数（头 + 最大像素区）
inline qsizetype slotBytes()
{
    return static_cast<qsizetype>(sizeof(ImagePlaneHeader)) + kMaxImageBytes;
}

// 整段 SHM 区域字节数
inline qsizetype regionBytes()
{
    return static_cast<qsizetype>(sizeof(ControlBlock)) + slotBytes() * kRingSlots;
}

// 控制块指针
inline ControlBlock *controlBlock(void *shm)
{
    return static_cast<ControlBlock *>(shm);
}

// 指定环槽起始地址
inline char *slotBase(void *shm, quint32 slotIndex)
{
    return static_cast<char *>(shm) + sizeof(ControlBlock) + slotBytes() * slotIndex;
}

// 环槽图像头指针
inline ImagePlaneHeader *planeHeader(void *shm, quint32 slotIndex)
{
    return reinterpret_cast<ImagePlaneHeader *>(slotBase(shm, slotIndex));
}

// 环槽像素 payload 指针
inline uchar *planePayload(void *shm, quint32 slotIndex)
{
    return reinterpret_cast<uchar *>(planeHeader(shm, slotIndex) + 1);
}

} // namespace CameraIpc

#endif // CAMERALIPCLAYOUT_H
