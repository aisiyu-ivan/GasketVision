#ifndef OKNGPIECHART_H
#define OKNGPIECHART_H

#include <QWidget>

// OK/NG 双扇区饼图（仅两档：合格 / 不合格）
class OkNgPieChart : public QWidget
{
    Q_OBJECT

public:
    // 构造饼图控件
    explicit OkNgPieChart(QWidget *parent = nullptr);

    // 设置 OK、NG 计数并触发重绘
    void setCounts(unsigned int ok, unsigned int ng);

    // 布局系统请求的最小尺寸
    QSize minimumSizeHint() const override;
    // 布局系统请求的推荐尺寸
    QSize sizeHint() const override;

protected:
    // 绘制饼图、中心总数与底部图例
    void paintEvent(QPaintEvent *event) override;

private:
    unsigned int m_ok = 0; // OK 计数
    unsigned int m_ng = 0; // NG 计数
};

#endif // OKNGPIECHART_H
