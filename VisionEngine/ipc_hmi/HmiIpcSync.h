#ifndef HMIIPCSYNC_H
#define HMIIPCSYNC_H

#include <QString>

// 引擎 ↔ HMI：互斥锁保护 SHM + 计数信号量通知新帧
class HmiIpcSync
{
public:
    // 构造同步对象；空参数时使用 HmiIpc 默认命名
    explicit HmiIpcSync(const QString &mutexName = QString(),
                          const QString &frameSemName = QString());
    // 释放 Mutex 与信号量句柄
    ~HmiIpcSync();

    // 引擎进程：创建 Mutex 与新帧信号量
    bool create();
    // 打开已有 Mutex 与信号量
    bool open();
    // 关闭并释放 Mutex / 信号量句柄
    void close();
    // 同步对象是否已成功 open/create
    bool isOpen() const;

    // 读写 SHM 前加锁
    bool lock(int timeoutMs = 5000);
    // 读写 SHM 后解锁
    void unlock();

    // 发布端：通知 HMI 有新帧
    void notifyFrameReady();
    // 订阅端：阻塞等待新帧信号量
    bool waitFrameReady(int timeoutMs);

private:
    // 对端崩溃导致 WAIT_ABANDONED 时尝试恢复
    bool recoverAbandoned();

    QString m_mutexName; // 互斥锁命名
    QString m_frameSemName; // 新帧信号量命名
    void *m_mutexHandle = nullptr; // Mutex 句柄
    void *m_frameSemHandle = nullptr; // 新帧信号量句柄
};

#endif // HMIIPCSYNC_H
