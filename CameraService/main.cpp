#include "CameraGrabWorker.h"
#include "CameraPublishWorker.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QThread>

Q_DECLARE_METATYPE(VisionFrame)

// 从 JSON 文件加载相机服务配置
static QJsonObject loadConfig(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

// CameraService 入口：主线程事件循环 + 采图线程 + 发布线程
int main(int argc, char *argv[])
{
    qRegisterMetaType<VisionFrame>("VisionFrame");

    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("CameraService"));

    QString configPath = QStringLiteral("vision_engine.json");
    if (app.arguments().size() > 1)
        configPath = app.arguments().at(1);

    const QJsonObject config = loadConfig(configPath);
    if (config.isEmpty())
        return 2;

    QThread grabThread;
    QThread publishThread;

    auto *grabWorker = new CameraGrabWorker;
    auto *publishWorker = new CameraPublishWorker;

    QObject::connect(grabWorker, &CameraGrabWorker::logMessage, [](const QString &msg) {
        qInfo("%s", qPrintable(msg));
    });
    QObject::connect(publishWorker, &CameraPublishWorker::logMessage, [](const QString &msg) {
        qInfo("%s", qPrintable(msg));
    });

    if (!grabWorker->configure(config))
        return 3;

    if (!publishWorker->configure(config))
        return 4;

    grabWorker->setPublishWorker(publishWorker);

    grabWorker->moveToThread(&grabThread);
    publishWorker->moveToThread(&publishThread);

    QObject::connect(&grabThread, &QThread::finished, grabWorker, &QObject::deleteLater);
    QObject::connect(&publishThread, &QThread::finished, publishWorker, &QObject::deleteLater);

    QObject::connect(grabWorker, &CameraGrabWorker::frameGrabbed, publishWorker,
                     &CameraPublishWorker::onFrameGrabbed, Qt::QueuedConnection);
    QObject::connect(publishWorker, &CameraPublishWorker::readyForNextGrab, grabWorker,
                     &CameraGrabWorker::onReadyForNextGrab, Qt::QueuedConnection);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        QMetaObject::invokeMethod(publishWorker, "stopPublish", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(grabWorker, "stopGrab", Qt::BlockingQueuedConnection);
        publishThread.quit();
        grabThread.quit();
        publishThread.wait();
        grabThread.wait();
    });

    grabThread.start();
    publishThread.start();

    bool publishOk = false;
    QMetaObject::invokeMethod(publishWorker, "startPublish", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, publishOk));
    if (!publishOk)
        return 5;

    bool grabOk = false;
    QMetaObject::invokeMethod(grabWorker, "startGrab", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, grabOk));
    if (!grabOk)
        return 6;

    return app.exec();
}
