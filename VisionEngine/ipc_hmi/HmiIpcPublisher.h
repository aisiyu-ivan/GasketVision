#ifndef HMIIPCPUBLISHER_H
#define HMIIPCPUBLISHER_H

#include "GasketInspector.h"
#include "HmiIpcLayout.h"

#include <QString>

namespace cv
{
class Mat;
}

// 引擎 → HMI：将检测结果与高清图写入环形共享内存，并通过命名事件通知 HMI
class HmiIpcPublisher
{
public:
    // 构造发布器（尚未 initialize）
    HmiIpcPublisher();
    // 释放 SHM 与同步对象
    ~HmiIpcPublisher();

    // 创建检测 SHM 区域与同步对象
    bool initialize();
    // 发布一帧检测结果（原图、标注图、测量值、OK/NG）
    bool publish(int stationId, const GasketInspectResult &result, const cv::Mat &originalImage,
                 const QString &cameraStatus);
    // 严格模式：按握手 granted 的 frameId 写入
    bool publish(int stationId, const GasketInspectResult &result, const cv::Mat &originalImage,
                 const QString &cameraStatus, quint64 frameId);
    // 最近一次 publish 写入 HMI SHM 的 frameId
    quint64 lastPublishedFrameId() const { return m_frameId; }

private:
    // 缺陷中文描述 → 协议枚举
    static HmiIpc::DefectCode defectCodeFromText(const QString &defect);
    // 将 Mat 写入环槽原图或标注图平面
    static bool writePlane(void *shm, quint32 slotIndex, bool annotated, quint64 frameId, const cv::Mat &image);

    QString m_shmKey; // SHM 区域键名
    class QSharedMemory *m_shm = nullptr; // 共享内存映射
    class HmiIpcSync *m_sync = nullptr; // 互斥锁与新帧信号量
    quint64 m_frameId = 0; // 单调递增帧序号
    bool m_ready = false; // initialize 是否成功
};

#endif // HMIIPCPUBLISHER_H
