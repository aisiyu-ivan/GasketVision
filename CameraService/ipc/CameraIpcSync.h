#ifndef CAMERAIPCSYNC_H
#define CAMERAIPCSYNC_H

#include <QString>

// 相机 IPC 跨进程同步：互斥锁保护 SHM + 计数信号量通知新帧
class CameraIpcSync
{
public:
    // 构造同步对象；空参数时使用 CameraIpc 默认命名
    explicit CameraIpcSync(const QString &mutexName = QString(),
                           const QString &frameSemName = QString());
    // 释放 Mutex 与信号量句柄
    ~CameraIpcSync();

    // 相机进程：创建 Mutex 与新帧信号量
    bool create();
    // 引擎进程：打开已存在的命名同步对象
    bool open();
    // 关闭并释放 Mutex / 信号量句柄
    void close();
    // 同步对象是否已成功 open/create
    bool isOpen() const;

    // 读写 SHM 前加锁
    bool lock(int timeoutMs = 5000);
    // 读写 SHM 后解锁
    void unlock();

    // 发布端：通知引擎有新帧
    void notifyFrameReady();
    // 订阅端：阻塞等待新帧信号量
    bool waitFrameReady(int timeoutMs);

private:
    // 对端崩溃导致 WAIT_ABANDONED 时尝试恢复
    bool recoverAbandoned();

    QString m_mutexName;       // 命名 Mutex 名
    QString m_frameSemName;    // 新帧信号量名
    void *m_mutexHandle = nullptr;    // Mutex 句柄（Windows HANDLE）
    void *m_frameSemHandle = nullptr; // 信号量句柄（Windows HANDLE）
};

#endif // CAMERAIPCSYNC_H
