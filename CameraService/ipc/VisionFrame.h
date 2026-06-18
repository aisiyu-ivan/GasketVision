#ifndef VISIONFRAME_H
#define VISIONFRAME_H

#include <QString>

#include <opencv2/core.hpp>

// 单帧采图数据（相机进程发布，检测引擎消费）
struct VisionFrame
{
    cv::Mat image;           // OpenCV 图像矩阵
    QString sourcePath;      // 样本或源文件路径
    qint64 timestampMs = 0;  // 采图时间戳（毫秒）
    QString cameraStatus;    // 相机状态描述
    quint64 frameId = 0;     // 帧序号（IPC 写入时填充）
    quint32 slotIndex = 0;   // 相机 SHM 槽索引
};

#endif // VISIONFRAME_H
