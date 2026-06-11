#include "OkNgStatsPanel.h"

#include "OkNgPieChart.h"

#include <QFile>
#include <QFont>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QSizePolicy>
#include <QVBoxLayout>

// 构造 OK/NG 统计面板并嵌入饼图
OkNgStatsPanel::OkNgStatsPanel(QWidget *parent)
    : QWidget(parent)
{
    m_counts.insert(QStringLiteral("OK"), 0U);
    m_counts.insert(QStringLiteral("NG"), 0U);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *title = new QLabel(QStringLiteral("OK/NG统计饼图"));
    QFont titleFont = title->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral(
        "background:transparent;color:#eee;padding:12px 8px 8px 8px;"));

    m_pieChart = new OkNgPieChart(this);
    m_pieChart->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    layout->addWidget(title);
    layout->addWidget(m_pieChart, 1);

    setStyleSheet(QStringLiteral("background:transparent;color:#eee;border:none;"));
    refreshChart();
}

// 更新 OK/NG 计数并刷新饼图
void OkNgStatsPanel::setCounts(const QHash<QString, unsigned int> &counts)
{
    m_counts = counts;
    if (!m_counts.contains(QStringLiteral("OK")))
        m_counts.insert(QStringLiteral("OK"), 0U);
    if (!m_counts.contains(QStringLiteral("NG")))
        m_counts.insert(QStringLiteral("NG"), 0U);
    refreshChart();
}

// 返回当前 OK/NG 计数副本
QHash<QString, unsigned int> OkNgStatsPanel::counts() const
{
    return m_counts;
}

// 从 JSON 文件加载初始计数
bool OkNgStatsPanel::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    const QJsonObject varCounts = root.value(QStringLiteral("varCounts")).toObject();
    if (varCounts.isEmpty())
        return false;

    QHash<QString, unsigned int> counts;
    for (auto it = varCounts.begin(); it != varCounts.end(); ++it)
        counts.insert(it.key(), static_cast<unsigned int>(it.value().toInt(0)));

    setCounts(counts);
    return true;
}

// 将当前计数保存为 JSON 文件
bool OkNgStatsPanel::saveToFile(const QString &path) const
{
    QJsonObject varCounts;
    for (auto it = m_counts.cbegin(); it != m_counts.cend(); ++it)
        varCounts.insert(it.key(), static_cast<int>(it.value()));

    QJsonObject root;
    root.insert(QStringLiteral("rows"), 1);
    root.insert(QStringLiteral("cols"), 1);
    root.insert(QStringLiteral("pieCount"), 1);
    root.insert(QStringLiteral("fillOrder"), QStringLiteral("RowMajor"));
    root.insert(QStringLiteral("varsPerPie"), QJsonArray{QJsonArray{QStringLiteral("OK"), QStringLiteral("NG")}});
    root.insert(QStringLiteral("varCounts"), varCounts);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

// 将 m_counts 同步到内嵌饼图
void OkNgStatsPanel::refreshChart()
{
    if (!m_pieChart)
        return;
    m_pieChart->setCounts(m_counts.value(QStringLiteral("OK"), 0U), m_counts.value(QStringLiteral("NG"), 0U));
}
