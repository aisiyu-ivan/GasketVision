#include "CameraIpcReader.h"

#include "CameraIpcLayout.h"
#include "CameraIpcSync.h"

#include <QDateTime>
#include <QSharedMemory>
#include <QThread>

#include <QtGlobal>

#include <opencv2/core.hpp>

#include <cstring>

// 构造相机 IPC 读取器
CameraIpcReader::CameraIpcReader()
    : m_sync(new CameraIpcSync())
{
}

// 释放 SHM 与同步对象
CameraIpcReader::~CameraIpcReader()
{
    if (m_shm)
    {
        if (m_shm->isAttached())
            m_shm->detach();
        delete m_shm;
        m_shm = nullptr;
    }
    delete m_sync;
    m_sync = nullptr;
}

// attach 相机 SHM 区域
bool CameraIpcReader::attachRegion()
{
    if (m_shm && m_shm->isAttached())
        return true;

    if (!m_sync->open())
        return false;

    m_shm = new QSharedMemory(CameraIpc::shmKey());
    if (!m_shm->attach())
        return false;
    if (m_shm->size() < static_cast<int>(CameraIpc::regionBytes()))
        return false;
    return true;
}

// 打开相机同步对象并 attach SHM
bool CameraIpcReader::initialize()
{
    if (m_ready)
        return true;
    if (!attachRegion())
        return false;
    m_ready = true;
    return true;
}

// 在已加锁前提下从 SHM 解析 VisionFrame
bool CameraIpcReader::readFrame(VisionFrame &out)
{
    if (!attachRegion() || !m_sync->lock())
        return false;

    const auto *ctrl = CameraIpc::controlBlock(m_shm->data());
    if (ctrl->magic != CameraIpc::kMagic || ctrl->frameId == 0 || ctrl->frameId == m_lastFrameId)
    {
        m_sync->unlock();
        return false;
    }

    const quint32 slot = ctrl->slotIndex % CameraIpc::kRingSlots;
    const auto *header = CameraIpc::planeHeader(m_shm->data(), slot);
    const uchar *payload = CameraIpc::planePayload(m_shm->data(), slot);

    if (header->frameId != ctrl->frameId || header->magic != CameraIpc::kMagic || header->payloadSize == 0)
    {
        m_sync->unlock();
        return false;
    }

    cv::Mat image;
    if (header->format == CameraIpc::Gray8)
    {
        image = cv::Mat(static_cast<int>(header->height), static_cast<int>(header->width), CV_8UC1,
                        const_cast<uchar *>(payload), static_cast<size_t>(header->bytesPerLine))
                      .clone();
    }
    else if (header->format == CameraIpc::Bgr888)
    {
        image = cv::Mat(static_cast<int>(header->height), static_cast<int>(header->width), CV_8UC3,
                        const_cast<uchar *>(payload), static_cast<size_t>(header->bytesPerLine))
                      .clone();
    }
    else
    {
        m_sync->unlock();
        return false;
    }

    out.image = image;
    out.sourcePath = QString::fromUtf8(ctrl->sourcePath);
    out.timestampMs = ctrl->timestampMs;
    out.cameraStatus = QString::fromUtf8(ctrl->cameraStatus);
    out.frameId = ctrl->frameId;
    m_lastFrameId = ctrl->frameId;

    m_sync->unlock();
    return true;
}

// 等待新帧信号量并读取一帧（去重 frameId；重复 notify 时继续等到新 frameId）
bool CameraIpcReader::waitAndRead(VisionFrame &out, int timeoutMs)
{
    if (!m_ready && !initialize())
        return false;

    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMax(timeoutMs, 1);
    while (QDateTime::currentMSecsSinceEpoch() < deadline)
    {
        const int remaining = static_cast<int>(deadline - QDateTime::currentMSecsSinceEpoch());
        if (!m_sync->waitFrameReady(qMax(remaining, 1)))
            return false;

        if (readFrame(out))
            return true;
    }
    return false;
}
