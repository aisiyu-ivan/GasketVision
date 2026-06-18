#ifndef CAMERALIPCPUBLISHER_H
#define CAMERALIPCPUBLISHER_H

#include "VisionFrame.h"

class IVisionImageSource;

// 相机进程：将 VisionFrame 写入环形存储区并通知检测引擎
class CameraIpcPublisher
{
public:
    // 构造发布器（尚未 initialize）
    CameraIpcPublisher();
    // 释放 SHM 与同步对象
    ~CameraIpcPublisher();

    // 创建相机 SHM 区域与同步对象
    bool initialize();
    // 将一帧写入环槽并通知引擎
    bool publish(const VisionFrame &frame);
    // 严格模式：按握手 granted 的 frameId 写入
    bool publish(const VisionFrame &frame, quint64 frameId);
    // 直写：像素已在 SHM payload 中，仅填头与 control（无 memcpy）
    bool publishDirect(quint64 frameId, quint8 format, quint32 width, quint32 height, quint32 bytesPerLine,
                       quint32 payloadSize, const VisionFrame &meta);
    // 采图直写 SHM 槽位（持锁 grabFrameInto，管道内第 1 次深拷）
    bool captureFromSource(quint64 frameId, IVisionImageSource &source);
    // 获取可直写的环槽 payload（调用方须已 hold mutex）
    static uchar *slotPayload(void *shm, quint32 slotIndex);
    // 最近一次 publish 的 frameId
    quint64 lastPublishedFrameId() const { return m_frameId; }
    // 严格模式：等待引擎在 control 块上标记指定帧已标注
    bool waitForAnnotated(quint64 frameId, int timeoutMs = 10000);
    class CameraIpcSync *sync() { return m_sync; }

private:
    // 将像素写入指定环槽并填充图像头
    bool writePlane(void *shm, quint32 slotIndex, quint64 frameId, const VisionFrame &frame);

    class QSharedMemory *m_shm = nullptr; // 相机 SHM 区域
    class CameraIpcSync *m_sync = nullptr; // 跨进程 Mutex / 信号量
    quint64 m_frameId = 0;                 // 已发布帧序号
    bool m_ready = false;                  // initialize 是否成功
};

#endif // CAMERALIPCPUBLISHER_H
