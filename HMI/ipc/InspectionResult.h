#ifndef INSPECTIONRESULT_H
#define INSPECTIONRESULT_H

#include <QImage>
#include <QString>

// HMI 侧单帧测量值（来自相机 SHM 控制块）
struct InspectionMeasurements
{
    double outerDiameterMm = 0.0;
    double innerDiameterMm = 0.0;
    double offsetXMm = 0.0;
    double offsetYMm = 0.0;
    double matchScore = 0.0;
};

// HMI 侧接收的单条检测结果（相机 SHM 控制块 + 标注图浅拷视图）
struct InspectionResult
{
    int stationId = 0;
    bool ok = false;
    QString defect;
    QString imagePath;
    QString annotatedImagePath;
    QString cameraStatus;
    InspectionMeasurements measurements;
    bool fromIpc = false;
    quint64 frameId = 0;
    QImage annotatedImage;
};

#endif // INSPECTIONRESULT_H
