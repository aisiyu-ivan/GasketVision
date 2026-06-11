#ifndef CAMERAGRABWORKER_H
#define CAMERAGRABWORKER_H

#include "IVisionImageSource.h"
#include "VisionFrame.h"

#include <QJsonObject>
#include <QObject>

#include <memory>

class QTimer;

// 采图线程：按间隔或严格反压抓取 VisionFrame
class CameraGrabWorker : public QObject
{
    Q_OBJECT

public:
    explicit CameraGrabWorker(QObject *parent = nullptr);
    ~CameraGrabWorker() override;

    bool configure(const QJsonObject &rootConfig);
    void setPublishWorker(class CameraPublishWorker *publishWorker);

public slots:
    bool startGrab();
    void stopGrab();
    void onReadyForNextGrab();

signals:
    void frameGrabbed(const VisionFrame &frame);
    void logMessage(const QString &message);

private slots:
    void grabOnce();
    void onIntervalTimer();

private:
    static int resolveIntervalMs(const QJsonObject &rootConfig);

    std::unique_ptr<IVisionImageSource> m_source;
    CameraPublishWorker *m_publishWorker = nullptr;
    bool m_strictAccounting = true;
    bool m_running = false;
    int m_intervalMs = 800;
    QTimer *m_intervalTimer = nullptr;
};

#endif // CAMERAGRABWORKER_H
