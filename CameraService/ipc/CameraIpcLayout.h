#ifndef CAMERALIPCLAYOUT_H
#define CAMERALIPCLAYOUT_H

#include <QtGlobal>
#include <QString>

// 相机 SHM：采图、原地标注、HMI 浅拷展示（单存储区）
namespace CameraIpc
{
constexpr quint32 kMagic = 0x47564349u; // 'GVCI'
constexpr quint32 kVersion = 4;
constexpr int kRingSlots = 4;
constexpr int kMaxImageBytes = 1920 * 1080 * 3;

enum ImageFormat : quint8
{
    Gray8 = 0,
    Bgr888 = 1,
};

enum DefectCode : quint8
{
    DefectNone = 0,
    DefectMissing = 1,
    DefectSize = 2,
    DefectEccentric = 3,
    DefectUnknown = 255,
};

inline QString shmKey()
{
    return QStringLiteral("GasketVision.Camera.Ipc.Region");
}

inline QString mutexName()
{
    return QStringLiteral("GasketVision.Camera.Ipc.Mutex");
}

inline QString frameSemName()
{
    return QStringLiteral("GasketVision.Camera.Ipc.FrameReady");
}

constexpr quint8 kFlagHasOriginal = 0x01;
constexpr quint8 kFlagHasAnnotated = 0x02;

#pragma pack(push, 1)
// 环槽内单平面图像头（Gray8 或 Bgr888 像素紧随其后）
struct ImagePlaneHeader
{
    quint32 magic = 0;
    quint64 frameId = 0;
    quint32 width = 0;
    quint32 height = 0;
    quint32 bytesPerLine = 0;
    quint8 format = 0;
    quint8 reserved[3] = {};
    quint32 payloadSize = 0;
};

// 全局控制块：当前帧序号、检测结果、路径及 flags（原图/标注）
struct ControlBlock
{
    quint32 magic = 0;
    quint32 version = 0;
    quint64 frameId = 0;
    qint64 timestampMs = 0;
    quint32 slotIndex = 0;
    quint32 stationId = 0;
    quint8 ok = 0;
    quint8 defectCode = 0;
    quint8 flags = 0;
    quint8 reserved = 0;
    double outerDiameterMm = 0.0;
    double innerDiameterMm = 0.0;
    double offsetXMm = 0.0;
    double offsetYMm = 0.0;
    double matchScore = 0.0;
    char defectText[64] = {};
    char cameraStatus[64] = {};
    char sourcePath[260] = {};
    char annotatedPath[260] = {};
};
#pragma pack(pop)

inline qsizetype slotBytes()
{
    return static_cast<qsizetype>(sizeof(ImagePlaneHeader)) + kMaxImageBytes;
}

inline qsizetype regionBytes()
{
    return static_cast<qsizetype>(sizeof(ControlBlock)) + slotBytes() * kRingSlots;
}

inline ControlBlock *controlBlock(void *shm)
{
    return static_cast<ControlBlock *>(shm);
}

inline char *slotBase(void *shm, quint32 slotIndex)
{
    return static_cast<char *>(shm) + sizeof(ControlBlock) + slotBytes() * slotIndex;
}

inline ImagePlaneHeader *planeHeader(void *shm, quint32 slotIndex)
{
    return reinterpret_cast<ImagePlaneHeader *>(slotBase(shm, slotIndex));
}

inline uchar *planePayload(void *shm, quint32 slotIndex)
{
    return reinterpret_cast<uchar *>(planeHeader(shm, slotIndex) + 1);
}

} // namespace CameraIpc

#endif // CAMERALIPCLAYOUT_H
