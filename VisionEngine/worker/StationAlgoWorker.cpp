#include "StationAlgoWorker.h"

#include "InspectDispatch.h"

#include <QDir>
#include <QFileInfo>

// 构造算法工作对象
StationAlgoWorker::StationAlgoWorker(QObject *parent)
    : QObject(parent)
{
}

// 加载检测器模板与工位参数
bool StationAlgoWorker::configure(const QJsonObject &rootConfig, int stationId, const QString &configDir)
{
    m_stationId = stationId;

    QJsonObject inspectorConfig = rootConfig;
    const QString templateRel = rootConfig.value(QStringLiteral("templatePath")).toString();
    if (!templateRel.isEmpty() && !QFileInfo(templateRel).isAbsolute())
    {
        const QString base = configDir.isEmpty() ? QDir::currentPath() : configDir;
        inspectorConfig.insert(QStringLiteral("templatePath"), QDir(base).filePath(templateRel));
    }

    if (!m_inspector.configure(inspectorConfig))
    {
        emit logMessage(QStringLiteral("检测器配置失败（模板未加载）"));
        return false;
    }

    return true;
}

void StationAlgoWorker::inspectDispatchAsync(const InspectDispatch &dispatch)
{
    const GasketInspectResult result =
        m_inspector.inspect(dispatch.frame.image, dispatch.frame.sourcePath, m_stationId,
                            const_cast<CameraAnnotTarget *>(&dispatch.annotTarget));
    emit inspectCompleted(result, dispatch.frame);
}
