#ifndef CAMERAANNOTTARGET_H
#define CAMERAANNOTTARGET_H

#include "CameraIpcLayout.h"

// 算法直写相机 SHM 槽位的目标句柄（引擎进程内有效）
struct CameraAnnotTarget
{
    quint64 frameId = 0;
    quint32 slotIndex = 0;
    CameraIpc::ImagePlaneHeader *header = nullptr;
    uchar *payload = nullptr;
    size_t capacity = 0;
};

#endif // CAMERAANNOTTARGET_H
