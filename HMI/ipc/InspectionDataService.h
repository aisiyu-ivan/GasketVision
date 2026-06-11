#ifndef INSPECTIONDATASERVICE_H
#define INSPECTIONDATASERVICE_H

#include "InspectionAggregator.h"
#include "HmiIpcSubscriber.h"
#include "InspectionResult.h"

#include <QObject>
#include <QTimer>

class InspectionWindow;

// 检测数据服务：通信线程收 SHM + 套接字 ack，主线程聚合 OK/NG 并刷 UI
class InspectionDataService : public QObject
{
    Q_OBJECT

public:
    explicit InspectionDataService(InspectionWindow *window, QObject *parent = nullptr);
    ~InspectionDataService() override;

    bool isClientConnected() const { return m_clientConnected; }
    const InspectionAggregator &aggregator() const { return m_aggregator; }
    void resetIpcConnection();
    void clearSession();
    // 须在 connect statusSocketListening 之后调用
    void startCommunication();

signals:
    void statusMessage(const QString &message);
    void statusSocketListening();
    void lastResult(const InspectionResult &result);

private slots:
    void onFrame(const InspectionResult &result);
    void flushUi();

private:
    void refreshWindow();

    InspectionWindow *m_window = nullptr;
    InspectionAggregator m_aggregator;
    HmiIpcSubscriber *m_hmiIpcSubscriber = nullptr;
    QTimer *m_flushTimer = nullptr;
    bool m_dirty = false;
    bool m_clientConnected = false;
    InspectionResult m_lastResult;
};

#endif // INSPECTIONDATASERVICE_H
