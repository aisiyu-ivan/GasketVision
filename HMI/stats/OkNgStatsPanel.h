#ifndef OKNGSTATSPANEL_H
#define OKNGSTATSPANEL_H

#include <QHash>
#include <QString>
#include <QWidget>

class OkNgPieChart;

// HMI 右侧 OK/NG 统计面板（嵌入 ui/OkNgPieChart，JSON 持久化）
class OkNgStatsPanel : public QWidget
{
    Q_OBJECT

public:
    // 构造面板并嵌入饼图控件
    explicit OkNgStatsPanel(QWidget *parent = nullptr);

    // 更新 OK/NG 计数并刷新饼图
    void setCounts(const QHash<QString, unsigned int> &counts);
    // 返回当前 OK/NG 计数副本
    QHash<QString, unsigned int> counts() const;

    // 从 JSON 文件加载初始计数
    bool loadFromFile(const QString &path);
    // 将当前计数保存为 JSON 文件
    bool saveToFile(const QString &path) const;

private:
    // 将 m_counts 同步到内嵌饼图
    void refreshChart();

    QHash<QString, unsigned int> m_counts; // OK/NG 计数表
    OkNgPieChart *m_pieChart = nullptr;  // 内嵌饼图控件
};

#endif // OKNGSTATSPANEL_H
