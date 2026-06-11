#include "ImageSourceFactory.h"

#include "GigEVisionCamera.h"
#include "SyntheticImageSource.h"

// 根据 imageSource 配置创建 SyntheticImageSource 或 GigEVisionCamera
std::unique_ptr<IVisionImageSource> ImageSourceFactory::create(const QJsonObject &config)
{
    const QString type = config.value(QStringLiteral("imageSource")).toString(QStringLiteral("synthetic")).toLower();
    if (type == QStringLiteral("gige"))
        return std::make_unique<GigEVisionCamera>();
    return std::make_unique<SyntheticImageSource>();
}
