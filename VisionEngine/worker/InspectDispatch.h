#ifndef INSPECTDISPATCH_H
#define INSPECTDISPATCH_H

#include "CameraAnnotTarget.h"
#include "VisionFrame.h"

// 通信线程派发给算法线程的单帧任务（含相机 SHM 标注槽）
struct InspectDispatch
{
    VisionFrame frame;
    CameraAnnotTarget annotTarget;
};

Q_DECLARE_METATYPE(InspectDispatch)

#endif // INSPECTDISPATCH_H
