#include "InspectionAggregator.h"

// 返回饼图扇区名称 OK、NG
QStringList InspectionAggregator::sectorNames()
{
    return {QStringLiteral("OK"), QStringLiteral("NG")};
}

// 将检测结果计入对应工位的 OK 或 NG 计数
void InspectionAggregator::record(const InspectionResult &result)
{
    if (result.stationId < 1)
        return;

    const QString bucket = result.ok ? QStringLiteral("OK") : QStringLiteral("NG");

    QMutexLocker lock(&m_mutex);
    m_perStation[result.stationId][bucket]++;
}

// 获取指定工位的 OK/NG 计数副本
QHash<QString, unsigned int> InspectionAggregator::countsForStation(int stationId) const
{
    QHash<QString, unsigned int> out;
    for (const QString &name : sectorNames())
        out.insert(name, 0U);

    QMutexLocker lock(&m_mutex);
    const auto it = m_perStation.constFind(stationId);
    if (it == m_perStation.cend())
        return out;

    for (auto jt = it->cbegin(); jt != it->cend(); ++jt)
        out[jt.key()] = jt.value();
    return out;
}

// 汇总所有工位的 OK/NG 总计
QHash<QString, unsigned int> InspectionAggregator::globalCounts() const
{
    QHash<QString, unsigned int> out;
    QMutexLocker lock(&m_mutex);
    for (auto st = m_perStation.cbegin(); st != m_perStation.cend(); ++st)
    {
        for (auto jt = st->cbegin(); jt != st->cend(); ++jt)
            out[jt.key()] += jt.value();
    }
    return out;
}

// 切换模式时清零所有工位计数
void InspectionAggregator::clear()
{
    QMutexLocker lock(&m_mutex);
    m_perStation.clear();
}
