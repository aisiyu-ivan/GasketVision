#ifndef PIECHARTCONFIGDIALOG_H
#define PIECHARTCONFIGDIALOG_H

#include <QDialog>

#include <QHash>
#include <QString>

#include "PieChartConfig.h"

class QComboBox;
class QFormLayout;
class QScrollArea;
class QSpinBox;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

class PieChartConfigDialog final : public QDialog // 配置对话框类
{
    Q_OBJECT

 // ---------- 构造与界面搭建----------

public:
    explicit PieChartConfigDialog(const PieChartConfig &initial, QWidget *parent = nullptr); // 构造函数,进行对话框布局

// ---------- 配置界面参数修改 ----------

private slots:
    void onGridDimsChanged(); // 行或列变化：按网格容量限制饼数上限
    void onPieCountChanged(int value); // 饼数变化：重建变量名编辑行与变量计数行

private:
    void rebuildVarLineEditors(); // 按当前饼数重建变量名编辑行
    void rebuildCountEditors(); // 按当前各饼变量名汇总重建「变量计数」行，并更新 m_dialogVarCounts

private slots:
    void onVarNamesTextChanged(); // 变量名编辑框文本变化：刷新变量计数行，使之与当前变量名集合一致

// ---------- 从配置界面读取并设置配置对象 ----------

private:
    bool applyUiToConfig(PieChartConfig &out) const; // 读取界面控件，写入传入的配置对象

public:
    PieChartConfig resultConfig() const; // 返回读取并写入配置对话框界面参数的配置对象

// ---------- 配置对象写入配置界面 ----------

private:
    void syncUiFromConfig(const PieChartConfig &cfg); // 将配置对象各字段写入静态控件；再重建动态编辑区并写入各饼变量名文本

// ---------- 另存为与从文件加载 ----------

public:
    QString preferredPersistPath() const; // 返回用户通过另存为或从文件加载指定的配置文件绝对路径

signals:
    void activeConfigFileChanged(const QString &absolutePath); // 另存为成功：通知主窗口更新「上次使用的配置文件」

private slots:
    void onSaveConfigAs(); // 另存为：按日期时间建议文件名，用户选路径后写入当前界面配置
    void onLoadConfigFromFile(); // 从文件加载：读入配置并刷新界面，确定后将保存到该路径

// ---------- 成员变量 ----------

private:
    QSpinBox *m_rowsSpin = nullptr;               // 指向行数编辑框控件的指针
    QSpinBox *m_colsSpin = nullptr;               // 指向列数编辑框控件的指针
    QSpinBox *m_pieCountSpin = nullptr;           // 指向饼数编辑框控件的指针
    QComboBox *m_fillOrderCombo = nullptr;        // 指向选择填充顺序的下拉框控件的指针：行优先或列优先
    QScrollArea *m_scroll = nullptr;              // 指向容纳变量名编辑框控件的滚动容器控件的指针
    QVBoxLayout *m_varLinesLayout = nullptr;      // 滚动容器控件内的内容控件的垂直布局
    QList<QLineEdit *> m_varLineEdits;            // 存储指向所有变量名编辑框控件指针的列表
    QScrollArea *m_countScroll = nullptr;         // 指向容纳变量计数编辑框控件的滚动容器控件的指针
    QWidget *m_countInner = nullptr;            // 指向滚动容器控件内的内容控件的指针
    QFormLayout *m_countsForm = nullptr;          // 指向变量名与变量计数编辑框控件成对排列的表单布局的指针
    QHash<QString, QSpinBox *> m_nameToCountSpin; // 存储所有数据对应的变量名与指向变量计数编辑框控件的指针的键值对容器
    QHash<QString, unsigned int> m_dialogVarCounts; // 存储所有数据对应的变量名与变量计数的键值对容器，是副本，只在编辑对话框时期使用
    QString m_targetPersistPath; // 用户通过另存为或从文件加载指定的配置文件绝对路径
};

#endif // PIECHARTCONFIGDIALOG_H
