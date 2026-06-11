#include "HmiIpcSync.h"

#include "HmiIpcLayout.h"

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace
{
constexpr int kMaxFrameSignals = 65535;
} // namespace

// 构造 HMI IPC 同步对象（命名 Mutex + 新帧信号量）
HmiIpcSync::HmiIpcSync(const QString &mutexName, const QString &frameSemName)
    : m_mutexName(mutexName.isEmpty() ? HmiIpc::mutexName() : mutexName)
    , m_frameSemName(frameSemName.isEmpty() ? HmiIpc::frameSemName() : frameSemName)
{
}

// 释放 Mutex 与信号量句柄
HmiIpcSync::~HmiIpcSync()
{
    close();
}

// 关闭并释放 Mutex / 信号量句柄
void HmiIpcSync::close()
{
#ifdef Q_OS_WIN
    if (m_frameSemHandle)
        CloseHandle(static_cast<HANDLE>(m_frameSemHandle));
    if (m_mutexHandle)
        CloseHandle(static_cast<HANDLE>(m_mutexHandle));
#endif
    m_frameSemHandle = nullptr;
    m_mutexHandle = nullptr;
}

// 同步对象是否已成功 open/create
bool HmiIpcSync::isOpen() const
{
    return m_mutexHandle != nullptr && m_frameSemHandle != nullptr;
}

// 引擎进程：创建 Mutex 与新帧信号量
bool HmiIpcSync::create()
{
#ifdef Q_OS_WIN
    HANDLE mutex = CreateMutexW(nullptr, FALSE, reinterpret_cast<LPCWSTR>(m_mutexName.utf16()));
    if (!mutex)
        return false;
    m_mutexHandle = mutex;

    HANDLE frame = CreateSemaphoreW(nullptr, 0, kMaxFrameSignals,
                                    reinterpret_cast<LPCWSTR>(m_frameSemName.utf16()));
    if (!frame)
        return false;
    m_frameSemHandle = frame;
    return true;
#else
    return open();
#endif
}

// 打开已有 Mutex 与信号量
bool HmiIpcSync::open()
{
    if (isOpen())
        return true;

    close();
#ifdef Q_OS_WIN
    HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, reinterpret_cast<LPCWSTR>(m_mutexName.utf16()));
    if (!mutex)
        return false;

    HANDLE frame = OpenSemaphoreW(SYNCHRONIZE, FALSE, reinterpret_cast<LPCWSTR>(m_frameSemName.utf16()));
    if (!frame)
    {
        CloseHandle(mutex);
        return false;
    }
    m_mutexHandle = mutex;
    m_frameSemHandle = frame;
    return true;
#else
    return false;
#endif
}

// 对端崩溃导致 WAIT_ABANDONED 时尝试恢复
bool HmiIpcSync::recoverAbandoned()
{
#ifdef Q_OS_WIN
    ReleaseMutex(static_cast<HANDLE>(m_mutexHandle));
    return true;
#else
    return false;
#endif
}

// 读写 SHM 前加锁
bool HmiIpcSync::lock(int timeoutMs)
{
#ifdef Q_OS_WIN
    if (!m_mutexHandle)
        return false;
    const DWORD r = WaitForSingleObject(static_cast<HANDLE>(m_mutexHandle), static_cast<DWORD>(timeoutMs));
    if (r == WAIT_OBJECT_0)
        return true;
    if (r == WAIT_ABANDONED)
        return recoverAbandoned();
    return false;
#else
    Q_UNUSED(timeoutMs);
    return false;
#endif
}

// 读写 SHM 后解锁
void HmiIpcSync::unlock()
{
#ifdef Q_OS_WIN
    if (m_mutexHandle)
        ReleaseMutex(static_cast<HANDLE>(m_mutexHandle));
#endif
}

// 发布端：递增新帧信号量
void HmiIpcSync::notifyFrameReady()
{
#ifdef Q_OS_WIN
    if (m_frameSemHandle)
        ReleaseSemaphore(static_cast<HANDLE>(m_frameSemHandle), 1, nullptr);
#endif
}

// 阻塞等待新帧信号量
bool HmiIpcSync::waitFrameReady(int timeoutMs)
{
#ifdef Q_OS_WIN
    if (!m_frameSemHandle)
        return false;
    const DWORD r = WaitForSingleObject(static_cast<HANDLE>(m_frameSemHandle), static_cast<DWORD>(timeoutMs));
    return r == WAIT_OBJECT_0;
#else
    Q_UNUSED(timeoutMs);
    return false;
#endif
}
