#include "InspectionDataService.h"

#include "InspectionWindow.h"

#include <QTimer>

InspectionDataService::InspectionDataService(InspectionWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
    qRegisterMetaType<InspectionResult>("InspectionResult");

    m_cameraIpcSubscriber = new CameraIpcSubscriber(this);
    connect(m_cameraIpcSubscriber, &CameraIpcSubscriber::listenReady, this, [this]() {
        emit statusMessage(QStringLiteral("HMI 本地套接字已就绪（Camera ↔ HMI）"));
        emit statusSocketListening();
    }, Qt::QueuedConnection);
    connect(m_cameraIpcSubscriber, &CameraIpcSubscriber::clientConnected, this, [this]() {
        m_clientConnected = true;
        if (m_cameraIpcSubscriber)
            m_cameraIpcSubscriber->resetConnection();
        emit statusMessage(QStringLiteral("CameraService 已连接（本地套接字）"));
    }, Qt::QueuedConnection);
    connect(m_cameraIpcSubscriber, &CameraIpcSubscriber::clientDisconnected, this, [this]() {
        m_clientConnected = false;
        emit statusMessage(QStringLiteral("CameraService 已断开（本地套接字）"));
    }, Qt::QueuedConnection);
    connect(m_cameraIpcSubscriber, &CameraIpcSubscriber::listenFailed, this,
            [this](const QString &reason) {
                emit statusMessage(QStringLiteral("HMI 本地套接字监听失败 — %1").arg(reason));
            },
            Qt::QueuedConnection);
    connect(m_cameraIpcSubscriber, &CameraIpcSubscriber::frameReceived, this, &InspectionDataService::onFrame);
    connect(m_cameraIpcSubscriber, &CameraIpcSubscriber::errorMessage, this, &InspectionDataService::statusMessage);
    connect(m_window, &InspectionWindow::ipcDisplayFinished, this, [this](quint64 frameId) {
        if (m_cameraIpcSubscriber)
            m_cameraIpcSubscriber->releaseDisplaySlot(frameId);
    });

    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(50);
    m_flushTimer->setSingleShot(true);
    connect(m_flushTimer, &QTimer::timeout, this, &InspectionDataService::flushUi);
}

InspectionDataService::~InspectionDataService() = default;

void InspectionDataService::resetIpcConnection()
{
    if (m_cameraIpcSubscriber)
        m_cameraIpcSubscriber->resetConnection();
}

void InspectionDataService::clearSession()
{
    if (m_flushTimer)
        m_flushTimer->stop();
    m_dirty = false;
    m_aggregator.clear();
    m_lastResult = InspectionResult();
}

void InspectionDataService::startCommunication()
{
    if (m_cameraIpcSubscriber)
        m_cameraIpcSubscriber->startComm();
}

void InspectionDataService::onFrame(const InspectionResult &result)
{
    m_aggregator.record(result);
    m_lastResult = result;
    m_dirty = true;
    emit lastResult(result);
    refreshWindow();
}

void InspectionDataService::flushUi()
{
    if (!m_dirty || !m_window)
        return;
    m_dirty = false;
    refreshWindow();
}

void InspectionDataService::refreshWindow()
{
    if (!m_window)
        return;
    m_window->applyInspectionResult(m_lastResult);
    m_window->setPieCounts(m_aggregator.countsForStation(m_lastResult.stationId));
}
