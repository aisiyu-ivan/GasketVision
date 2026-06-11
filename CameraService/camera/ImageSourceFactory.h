#ifndef IMAGESOURCEFACTORY_H
#define IMAGESOURCEFACTORY_H

#include "IVisionImageSource.h"

#include <QJsonObject>
#include <memory>

// 采图源工厂：根据配置 imageSource 字段创建对应实现
class ImageSourceFactory
{
public:
    // 创建 synthetic 或 gige 采图源实例
    static std::unique_ptr<IVisionImageSource> create(const QJsonObject &config);
};

#endif // IMAGESOURCEFACTORY_H
