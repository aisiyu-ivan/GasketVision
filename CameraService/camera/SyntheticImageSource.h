#ifndef SYNTHETICIMAGESOURCE_H
#define SYNTHETICIMAGESOURCE_H

#include "IVisionImageSource.h"

#include <QStringList>

// 合成采图源：循环读取本地样本目录中的图片文件
class SyntheticImageSource : public IVisionImageSource
{
public:
    // 扫描样本目录并按缺陷类型交错排序
    bool open(const QJsonObject &config) override;
    // 清空文件列表并标记为未打开
    void close() override;
    // 是否已成功加载样本文件
    bool isConnected() const override;
    // 返回合成相机状态描述
    QString statusText() const override;
    // 按序读取下一张样本图
    bool grabFrame(VisionFrame &out) override;

private:
    QStringList m_files; // 已排序的样本文件路径列表
    int m_index = 0;     // 当前播放索引
    int m_intervalMs = 800; // 配置中的帧间隔（毫秒，供外部读取）
    bool m_open = false; // 是否已成功打开样本目录
};

#endif // SYNTHETICIMAGESOURCE_H
