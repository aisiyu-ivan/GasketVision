#ifndef CAMERALIPCPUBLISHER_H
#define CAMERALIPCPUBLISHER_H

#include "VisionFrame.h"

// 相机进程：将 VisionFrame 写入环形 SHM 并通知检测引擎
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
    // 最近一次 publish 的 frameId
    quint64 lastPublishedFrameId() const { return m_frameId; }

private:
    // 将像素写入指定环槽并填充图像头
    bool writePlane(void *shm, quint32 slotIndex, quint64 frameId, const VisionFrame &frame);

    class QSharedMemory *m_shm = nullptr; // 相机 SHM 区域
    class CameraIpcSync *m_sync = nullptr; // 跨进程 Mutex / 信号量
    quint64 m_frameId = 0;                 // 已发布帧序号
    bool m_ready = false;                  // initialize 是否成功
};

#endif // CAMERALIPCPUBLISHER_H
