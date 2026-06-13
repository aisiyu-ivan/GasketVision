#ifndef CAMERAGRABWORKER_H
#define CAMERAGRABWORKER_H

#include "IVisionImageSource.h"
#include "VisionFrame.h"

#include <QJsonObject>
#include <QObject>

#include <memory>

class QTimer;

// 采图线程：按间隔持续抓取 VisionFrame（环缓反压在发布写 SHM 前）
class CameraGrabWorker : public QObject
{
    Q_OBJECT

public:
    explicit CameraGrabWorker(QObject *parent = nullptr);
    ~CameraGrabWorker() override;

    bool configure(const QJsonObject &rootConfig);

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
    bool m_strictAccounting = true;
    bool m_running = false;
    int m_intervalMs = 800;
    QTimer *m_intervalTimer = nullptr;
};

#endif // CAMERAGRABWORKER_H
