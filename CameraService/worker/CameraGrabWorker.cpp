#include "CameraGrabWorker.h"

#include "ImageSourceFactory.h"

#include <QJsonArray>
#include <QMetaObject>
#include <QTimer>

CameraGrabWorker::CameraGrabWorker(QObject *parent)
    : QObject(parent)
    , m_intervalTimer(new QTimer(this))
{
    m_intervalTimer->setSingleShot(false);
    connect(m_intervalTimer, &QTimer::timeout, this, &CameraGrabWorker::onIntervalTimer);
}

CameraGrabWorker::~CameraGrabWorker()
{
    stopGrab();
}

int CameraGrabWorker::resolveIntervalMs(const QJsonObject &rootConfig)
{
    const QString imageSource = rootConfig.value(QStringLiteral("imageSource")).toString().toLower();
    int intervalMs = 800;
    if (imageSource == QStringLiteral("gige"))
    {
        intervalMs = rootConfig.value(QStringLiteral("camera")).toObject().value(QStringLiteral("grabIntervalMs")).toInt(800);
    }
    else
    {
        intervalMs = rootConfig.value(QStringLiteral("synthetic")).toObject().value(QStringLiteral("intervalMs")).toInt(800);
        const QJsonArray stations = rootConfig.value(QStringLiteral("stations")).toArray();
        for (const QJsonValue &v : stations)
        {
            const QJsonObject st = v.toObject();
            if (st.value(QStringLiteral("stationId")).toInt() == 1)
            {
                intervalMs = st.value(QStringLiteral("intervalMs")).toInt(intervalMs);
                break;
            }
        }
    }
    return intervalMs > 0 ? intervalMs : 800;
}

bool CameraGrabWorker::configure(const QJsonObject &rootConfig)
{
    m_strictAccounting = rootConfig.value(QStringLiteral("strictSampleAccounting")).toBool(true);
    m_intervalMs = resolveIntervalMs(rootConfig);

    m_source = ImageSourceFactory::create(rootConfig);
    if (!m_source || !m_source->open(rootConfig))
        return false;

    m_intervalTimer->setInterval(m_intervalMs);
    return true;
}

void CameraGrabWorker::grabOnce()
{
    if (!m_running || !m_source)
        return;

    VisionFrame frame;
    if (!m_source->grabFrame(frame))
    {
        if (m_strictAccounting)
            QTimer::singleShot(m_intervalMs, this, &CameraGrabWorker::onReadyForNextGrab);
        return;
    }

    frame.cameraStatus = m_source->statusText();
    emit frameGrabbed(frame);
}

void CameraGrabWorker::onIntervalTimer()
{
    grabOnce();
}

void CameraGrabWorker::onReadyForNextGrab()
{
    if (!m_running)
        return;
    grabOnce();
}

bool CameraGrabWorker::startGrab()
{
    if (m_running)
        return true;
    if (!m_source)
        return false;

    m_running = true;
    emit logMessage(QStringLiteral("采图线程已启动"));
    m_intervalTimer->start();
    grabOnce();
    return true;
}

void CameraGrabWorker::stopGrab()
{
    m_running = false;
    if (m_intervalTimer)
        m_intervalTimer->stop();
    if (m_source)
        m_source->close();
}
