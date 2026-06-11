#include "OkNgPieChart.h"

#include <QPainter>
#include <QtMath>

namespace
{
constexpr int kStartAngle = 90 * 16;
constexpr int kFullAngle = 360 * 16;
constexpr int kLegendRows = 3;
constexpr int kLegendGap = 12;
constexpr int kSidePad = 12;
constexpr int kBottomPad = 12;
constexpr int kSwatch = 12;
constexpr int kSwatchTextGap = 8;
const QColor kOkColor(QStringLiteral("#2ecc71"));
const QColor kNgColor(QStringLiteral("#e74c3c"));
const QColor kEmptyColor(80, 80, 80);
} // namespace

// 构造 OK/NG 饼图控件
OkNgPieChart::OkNgPieChart(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(160, 160);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

// 设置 OK、NG 计数并触发重绘
void OkNgPieChart::setCounts(unsigned int ok, unsigned int ng)
{
    m_ok = ok;
    m_ng = ng;
    update();
}

// 布局系统请求的最小尺寸
QSize OkNgPieChart::minimumSizeHint() const
{
    return {160, 160};
}

// 布局系统请求的推荐尺寸
QSize OkNgPieChart::sizeHint() const
{
    return {220, 320};
}

// 绘制饼图、中心总数与底部图例
void OkNgPieChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.fillRect(rect(), QColor(47, 47, 47));

    const QFontMetrics fm(p.font());
    const int rowH = fm.height() + 6;
    const int legendBlockH = kLegendGap + kLegendRows * rowH;
    const int legendTop = height() - kBottomPad - legendBlockH + kLegendGap;

    const unsigned int total = m_ok + m_ng;
    const double okPct = total > 0 ? 100.0 * m_ok / total : 0.0;
    const double ngPct = total > 0 ? 100.0 * m_ng / total : 0.0;

    const QString okLabel = QStringLiteral("OK: %1 (%2%)").arg(m_ok).arg(okPct, 0, 'f', 1);
    const QString ngLabel = QStringLiteral("NG: %1 (%2%)").arg(m_ng).arg(ngPct, 0, 'f', 1);
    const QString sampleLabel = QStringLiteral("样品数: %1").arg(total);

    const int legendW = kSwatch + kSwatchTextGap
                        + qMax(fm.horizontalAdvance(okLabel),
                               qMax(fm.horizontalAdvance(ngLabel), fm.horizontalAdvance(sampleLabel)));
    const int legendX = (width() - legendW) / 2;
    const int textX = legendX + kSwatch + kSwatchTextGap;

    const int pieAreaTop = kSidePad;
    const int pieAreaBottom = legendTop - kLegendGap;
    const int pieAreaH = qMax(0, pieAreaBottom - pieAreaTop);
    const int maxSide = qMin(width() - kSidePad * 2, pieAreaH);
    if (maxSide <= 0)
        return;

    const int pieX = (width() - maxSide) / 2;
    const int pieY = pieAreaTop + (pieAreaH - maxSide) / 2;
    const QRect pieRect(pieX, pieY, maxSide, maxSide);

    if (total == 0)
    {
        p.setPen(QPen(QColor(120, 120, 120), 1));
        p.setBrush(kEmptyColor);
        p.drawEllipse(pieRect);
        p.setPen(QColor(180, 180, 180));
        p.drawText(pieRect, Qt::AlignCenter, QStringLiteral("暂无数据"));
    }
    else
    {
        const int okSpan = static_cast<int>(qRound(static_cast<double>(kFullAngle) * m_ok / total));
        const int ngSpan = kFullAngle - okSpan;

        p.setPen(QPen(Qt::white, 1));
        p.setBrush(kOkColor);
        p.drawPie(pieRect, kStartAngle, okSpan);
        p.setBrush(kNgColor);
        p.drawPie(pieRect, kStartAngle + okSpan, ngSpan);

        p.setPen(Qt::white);
        p.drawText(pieRect, Qt::AlignCenter, QStringLiteral("共 %1").arg(total));
    }

    auto drawSwatchRow = [&](int row, const QColor &color, const QString &label) {
        const int y = legendTop + row * rowH;
        const int swY = y + (rowH - kSwatch) / 2;
        const QRect sw(legendX, swY, kSwatch, kSwatch);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(sw, 2, 2);
        p.setPen(Qt::white);
        p.drawText(textX, y, width() - textX - kSidePad, rowH, Qt::AlignVCenter | Qt::AlignLeft, label);
    };

    drawSwatchRow(0, kOkColor, okLabel);
    drawSwatchRow(1, kNgColor, ngLabel);

    const int sampleY = legendTop + rowH * 2;
    p.setPen(QColor(200, 200, 200));
    p.drawText(textX, sampleY, width() - textX - kSidePad, rowH, Qt::AlignVCenter | Qt::AlignLeft, sampleLabel);
}
