#ifndef SINGLEPIECHART_H
#define SINGLEPIECHART_H

#include <QColor>
#include <QHash>
#include <QList>
#include <QMargins>
#include <QStringList>
#include <QWidget>

#include "VarCountData.h"

class QFontMetrics;
class QPaintEvent;
class QPainter;

class SinglePieChart : public QWidget// 单饼图类
{

// ---------- 构造与生命周期 ----------

public:
    explicit SinglePieChart(QWidget *parent = nullptr); // 构造函数
    ~SinglePieChart() override; // 析构函数

// ---------- 将从配置对象读取的数据设置到单饼图，并设置扇区颜色----------

public:
    void setSectorNamesAndColors(const QStringList &names); // 设置扇区变量名与颜色
    void applySectorDataCountsFromHash(const QHash<QString, unsigned int> &counts); //设置数据变量计数

// ---------- 尺寸提示 ----------

public:
    QSize minimumSizeHint() const override; // 返回建议最小尺寸
    QSize sizeHint() const override; // 返回默认建议尺寸

// ---------- 控件上方：标题栏区域 ----------

public:
    void setPieChartOrdinal(int oneBasedIndex); // 设置标题栏中的饼图序号

private:
    static int headerBarHeight(int widgetHeight); // 标题栏高度
    QRect headerBarRect() const; // 控件上方标题栏区域
    void drawHeaderBar(QPainter &p) const; // 绘制标题栏

// ---------- 控件下方：扣除标题栏、并扣除留白后的内容区域 ----------

private:
    QRect bodyContentRect() const; // 控件下方扣除标题栏、并扣除留白后的内容区域

// ---------- 控件下方左侧：图例区域 ----------

private:
    int legendTheoreticalWidth(const QFontMetrics &fm) const; // 图例理论宽度
    int legendReservedWidth() const; // 图例实际宽度
    QRect leftContentRect() const; // 控件下方左侧的图例区域
    void drawLeftLegend(QPainter &p) const; // 绘制图例

// ---------- 控件下方右侧：饼图区域 ----------

private:
    QRect rightContentRect() const; // 控件下方右侧的内容区域
    QRect pieRect() const; // 控件下方右侧的内容区域内居中的正方形饼图区域
    unsigned int totalCount() const; // 总计数：各扇区对应数据的变量计数之和
    QColor sectorColor(int index) const; // 返回当前扇区颜色
    void drawPieInnerSectorNames(QPainter &p, const QRect &pr, const QList<int> &start16, const QList<int> &span16) const; // 在饼图内绘制饼图不同扇区对应数据的变量名
    void drawPieOuterSectorLabels(QPainter &p, const QRect &pr, const QList<int> &start16, const QList<int> &span16, unsigned int total) const; // 在饼图外绘制饼图不同扇区对应数据的引线、箭头、变量名、变量计数占总计数的百分比
    void drawRightPieChart(QPainter &p) const; // 绘制饼图

// ---------- 绘制饼图：标题栏 → 左侧图例 → 右侧饼图 ----------

protected:
    void paintEvent(QPaintEvent *event) override; // 按顺序绘制饼图：标题栏、左侧图例、右侧饼图


// ---------- 成员变量 ----------
private:
    int m_pieOrdinal = 1; // 标题栏的饼图序号
    QList<VarCountData> m_sectorData; // 存储饼图不同扇区对应数据的列表
    QList<QColor> m_sectorColors; // 存储饼图不同扇区对应颜色的列表
    QStringList m_sectorDataNames; // 存储饼图不同扇区对应数据变量名的列表
    int m_startAngle; // 饼图第一个扇区起始角
    QMargins m_margins; // 饼图四周与控件边界的留白
};

#endif // SINGLEPIECHART_H
