#ifndef WIDGET_H
#define WIDGET_H

#include <QList>
#include <QPair>
#include <QString>
#include <QWidget>

#include <vector>

#include "PieChartConfig.h"
#include "VarCountData.h"

class QGridLayout;
class QPushButton;
class SinglePieChart;


class Widget : public QWidget // 主窗口类
{
    Q_OBJECT

// ---------- 窗口布局 ----------

public:
    explicit Widget(QWidget *parent = nullptr,//构造函数，主程序只传入父窗口指针，测试程序传入父窗口指针、组织名、应用名、文件名
                      const QString &settingsOrganization = QString(), // 组织名
                      const QString &settingsApplication = QString(), // 应用名
                      const QString &defaultConfigLeafName = QString()); // 默认配置文件名

// ---------- 对外接口 ----------

public:
    const PieChartConfig &config() const { return m_config; } // 返回当前配置对象(只读引用)
    PieChartConfig readConfiguration() const; // 返回当前配置对象副本
    void initializeConfiguration(const PieChartConfig &configuration); // 配置参数的初始化写入
    void applyExternalVarCountData(const std::vector<VarCountData> &data); // 设置饼图数据

public slots:
    void applyExternalVarsPerPieAndCounts(const QList<QStringList> &varsPerPie, const QHash<QString, unsigned int> &counts); // 外部写入变量名列表与变量计数并刷新各饼图

// ---------- 槽函数：响应配置按钮，打开对话框 ----------

private slots:
    void onOpenConfig(); // 槽函数：响应配置按钮，打开对话框


// ---------- 文件转换为配置对象 ----------

private:
    QString defaultConfigFilePath() const;        // 默认配置文件绝对路径
    QString activePersistPath() const;            // 读取上次使用的配置文件绝对路径
    void setActivePersistPath(const QString &path); // 设置上次使用的配置文件绝对路径
    void loadPersistedConfigOrDefaults();         // 启动时加载文件转换为配置对象

// ---------- 配置对象转换为文件 ----------

private:
    void saveConfigToFile(const QString &dialogPreferredPath); // 设置配置对象转换为的文件路径

// ---------- 演示用饼图 ----------

private:
    void setDemoConfig();                         // 设置演示用配置
    void setDemoCounts();                         // 设置演示用变量计数，并插入键值对容器
    void ensureValidConfigAndCounts();             // 保证配置合法且变量名都有计数，否则使用演示配置并把变量计数补全为0


// ---------- 饼图重建 ----------

private:
    QPair<int, int> slotToGrid(int slotIndex) const; // 将饼槽下标换算为网格中的 (行, 列)
    void rebuildGrid();                           // 销毁旧饼图子控件并按配置重建
    void pushDataToCharts();                      // 把配置与计数同步到各饼图控件
    void updateWindowMinimumSize();               // 防止窗口过小导致饼图不可读

// ---------- 成员变量 ----------

private:
    QString m_settingsOrg;                      // 组织名
    QString m_settingsApp;                      // 应用名
    QString m_defaultConfigLeaf;               // 默认配置文件名
    PieChartConfig m_config;                    // 配置对象,存储配置参数
    QPushButton *m_configButton = nullptr;      // 指向配置按钮的指针
    QGridLayout *m_gridLayout = nullptr;        // 指向饼图网格区域布局的指针
    QWidget *m_gridHost = nullptr;              // 指向饼图网格区域控件的指针
    QList<SinglePieChart *> m_charts;           // 存储指向各饼图指针的列表
};

#endif // WIDGET_H
