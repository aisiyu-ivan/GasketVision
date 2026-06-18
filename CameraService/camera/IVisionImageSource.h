#ifndef IVISIONIMAGESOURCE_H
#define IVISIONIMAGESOURCE_H

#include "VisionFrame.h"

#include <QJsonObject>
#include <QString>

// 采图源抽象接口（合成回放或 GigE 相机）
class IVisionImageSource
{
public:
    // 释放采图源
    virtual ~IVisionImageSource() = default;
    // 按配置打开采图源
    virtual bool open(const QJsonObject &config) = 0;
    // 关闭并释放资源
    virtual void close() = 0;
    // 是否已成功连接/加载
    virtual bool isConnected() const = 0;
    // 供 HMI 状态栏显示的状态文案
    virtual QString statusText() const = 0;
    // 抓取下一帧并填入 out
    virtual bool grabFrame(VisionFrame &out) = 0;
    // 直写下一帧像素到 dst（容量 capacity 字节），元数据写入 out；默认回退 grabFrame+memcpy
    virtual bool grabFrameInto(uchar *dst, int capacity, VisionFrame &out);
};

#endif // IVISIONIMAGESOURCE_H
