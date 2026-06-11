#ifndef CAMERALIPCREADER_H
#define CAMERALIPCREADER_H

#include "VisionFrame.h"

// 引擎 ↔ 相机进程：从相机环形 SHM 读取 VisionFrame
class CameraIpcReader
{
public:
    CameraIpcReader();
    ~CameraIpcReader();

    bool initialize();
    bool waitAndRead(VisionFrame &out, int timeoutMs = 500);
    quint64 lastConsumedFrameId() const { return m_lastFrameId; }

private:
    bool attachRegion();
    bool readFrame(VisionFrame &out);

    class QSharedMemory *m_shm = nullptr;
    class CameraIpcSync *m_sync = nullptr;
    quint64 m_lastFrameId = 0;
    bool m_ready = false;
};

#endif // CAMERALIPCREADER_H
