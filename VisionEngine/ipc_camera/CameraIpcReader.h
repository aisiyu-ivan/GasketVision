#ifndef CAMERALIPCREADER_H
#define CAMERALIPCREADER_H

#include "CameraAnnotTarget.h"
#include "GasketInspector.h"
#include "VisionFrame.h"

// 引擎 ↔ 相机进程：从相机环形存储区浅拷读取并在槽位原地标注
class CameraIpcReader
{
public:
    CameraIpcReader();
    ~CameraIpcReader();

    bool initialize();
    bool waitForRegion(int timeoutMs = 60000);
    bool waitAndRead(VisionFrame &out, int timeoutMs = 500);
    bool prepareAnnotTarget(quint32 slotIndex, quint64 frameId, CameraAnnotTarget &out);
    bool finishInspectPublish(int stationId, const GasketInspectResult &result, const QString &cameraStatus,
                              quint64 frameId, quint32 slotIndex);
    quint64 lastConsumedFrameId() const { return m_lastFrameId; }

private:
    bool attachRegion();
    bool readFrame(VisionFrame &out);
    static CameraIpc::DefectCode defectCodeFromText(const QString &defect);

    class QSharedMemory *m_shm = nullptr;
    class CameraIpcSync *m_sync = nullptr;
    quint64 m_lastFrameId = 0;
    bool m_ready = false;
};

#endif // CAMERALIPCREADER_H
