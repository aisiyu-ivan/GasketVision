#ifndef INSPECTIONRESULT_H
#define INSPECTIONRESULT_H

#include <QImage>
#include <QString>

// 检测测量值（外径、内径、偏心偏移及模板匹配分数）
struct InspectionMeasurements
{
    double outerDiameterMm = 0.0; // 外径（毫米）
    double innerDiameterMm = 0.0; // 内径（毫米）
    double offsetXMm = 0.0;       // X 方向偏心偏移（毫米）
    double offsetYMm = 0.0;       // Y 方向偏心偏移（毫米）
    double matchScore = 0.0;      // 模板匹配分数
};

// HMI 侧接收的单条检测结果（控制 SHM + 可选图像）
struct InspectionResult
{
    int stationId = 0;              // 工位编号
    bool ok = false;                  // 检测是否合格
    QString defect;                   // 缺陷描述
    QString imagePath;                // 原图文件路径
    QString annotatedImagePath;       // 标注图文件路径
    QString cameraStatus;             // 相机状态文本
    InspectionMeasurements measurements; // 测量值
    bool fromIpc = false;             // 是否来自 IPC 共享内存
    quint64 frameId = 0;              // 帧序号
    QImage originalImage;             // 原图（IPC 内嵌像素）
    QImage annotatedImage;            // 标注图（IPC 内嵌像素）
};

#endif // INSPECTIONRESULT_H
