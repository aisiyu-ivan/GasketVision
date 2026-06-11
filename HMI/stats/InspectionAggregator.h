#ifndef INSPECTIONAGGREGATOR_H
#define INSPECTIONAGGREGATOR_H

#include "InspectionResult.h"

#include <QHash>
#include <QMutex>
#include <QString>

// OK/NG 计数聚合器：按工位累计检测结果
class InspectionAggregator
{
public:
    // 记录一条检测结果到对应工位的 OK 或 NG 桶
    void record(const InspectionResult &result);
    // 获取指定工位的 OK/NG 计数（缺失项补零）
    QHash<QString, unsigned int> countsForStation(int stationId) const;
    // 获取所有工位的全局 OK/NG 合计
    QHash<QString, unsigned int> globalCounts() const;
    // 切换模式时清零所有工位计数
    void clear();

    // 饼图扇区名称列表（OK、NG）
    static QStringList sectorNames();

private:
    mutable QMutex m_mutex;                               // 保护 m_perStation 的互斥锁
    QHash<int, QHash<QString, unsigned int>> m_perStation; // 工位 → OK/NG 计数
};

#endif // INSPECTIONAGGREGATOR_H
