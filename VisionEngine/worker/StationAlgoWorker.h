#ifndef STATIONALGOWORKER_H
#define STATIONALGOWORKER_H

#include "GasketInspector.h"
#include "VisionFrame.h"

#include <QJsonObject>
#include <QObject>

// 算法线程：仅执行 GasketInspector，不参与 IPC 与套接字
class StationAlgoWorker : public QObject
{
    Q_OBJECT

public:
    explicit StationAlgoWorker(QObject *parent = nullptr);

    // 加载检测器模板与工位参数
    bool configure(const QJsonObject &rootConfig, int stationId, const QString &configDir = QString());

public slots:
    // 在算法线程执行单帧检测（异步，完成后 emit inspectCompleted）
    void inspectFrameAsync(const VisionFrame &frame);
    // 同步检测（保留供测试或外部调用）
    GasketInspectResult inspectFrame(const VisionFrame &frame);

signals:
    void logMessage(const QString &message);
    void inspectCompleted(const GasketInspectResult &result, const VisionFrame &frame);

private:
    int m_stationId = 1;
    GasketInspector m_inspector;
};

#endif // STATIONALGOWORKER_H
