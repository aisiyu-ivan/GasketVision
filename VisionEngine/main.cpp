#include "StationAlgoWorker.h"
#include "StationCommWorker.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QThread>

Q_DECLARE_METATYPE(VisionFrame)
Q_DECLARE_METATYPE(GasketInspectResult)

// 从 JSON 文件加载 VisionEngine 配置
static QJsonObject loadConfig(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.object();
}

// VisionEngine 入口：主线程事件循环 + 通信线程 + 算法线程
int main(int argc, char *argv[])
{
    qRegisterMetaType<VisionFrame>("VisionFrame");
    qRegisterMetaType<GasketInspectResult>("GasketInspectResult");

    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("VisionEngine"));

    QString configPath = QStringLiteral("vision_engine.json");
    if (app.arguments().size() > 1)
        configPath = app.arguments().at(1);

    const QJsonObject config = loadConfig(configPath);
    if (config.isEmpty())
        return 2;

    const QString configDir = QFileInfo(configPath).absolutePath();
    const bool strictAccounting = config.value(QStringLiteral("strictSampleAccounting")).toBool(true);

    QThread commThread;
    QThread algoThread;

    auto *commWorker = new StationCommWorker;
    auto *algoWorker = new StationAlgoWorker;

    commWorker->setStrictAccounting(strictAccounting);

    QObject::connect(commWorker, &StationCommWorker::logMessage, [](const QString &msg) {
        qInfo("%s", qPrintable(msg));
    });
    QObject::connect(algoWorker, &StationAlgoWorker::logMessage, [](const QString &msg) {
        qInfo("%s", qPrintable(msg));
    });

    if (!algoWorker->configure(config, 1, configDir))
        return 3;

    if (!commWorker->configure(config, 1, algoWorker, configDir))
        return 3;

    commWorker->moveToThread(&commThread);
    algoWorker->moveToThread(&algoThread);

    QObject::connect(&commThread, &QThread::finished, commWorker, &QObject::deleteLater);
    QObject::connect(&algoThread, &QThread::finished, algoWorker, &QObject::deleteLater);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        QMetaObject::invokeMethod(commWorker, "stopComm", Qt::BlockingQueuedConnection);
        commThread.quit();
        algoThread.quit();
        commThread.wait();
        algoThread.wait();
    });

    commThread.start();
    algoThread.start();

    bool commOk = false;
    QMetaObject::invokeMethod(commWorker, "startComm", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, commOk));
    if (!commOk)
        return 4;

    bool pipelineOk = false;
    QMetaObject::invokeMethod(commWorker, "startPipeline", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, pipelineOk));
    if (!pipelineOk)
        return 5;

    return app.exec();
}
