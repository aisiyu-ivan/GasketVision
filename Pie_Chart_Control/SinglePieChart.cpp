#include "SinglePieChart.h"

#include <cmath>

#include <QtMath>

#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>

// ---------- 构造与生命周期 ----------

SinglePieChart::SinglePieChart(QWidget *parent) //构造函数
    : QWidget(parent)
    , m_startAngle(90 * 16)// 将饼图的第一个扇形的起始边设定在90度位置
    , m_margins(8, 8, 8, 8)
{
}

SinglePieChart::~SinglePieChart() = default; // 析构函数

// ---------- 将从配置对象读取的数据设置到单饼图,并设置扇区颜色 ----------

namespace
{
QString sectorLabelText(const VarCountData &v)
{
    return QStringLiteral("%1: %2").arg(v.name, QString::number(v.count));
}

QList<QColor> sectorColorsForCount(int count) // 按扇区个数生成扇区颜色列表
{
    static const QList<QColor> kBase = // 固定调色板列表
        {
            QColor(QStringLiteral("#2ecc71")), // 绿
            QColor(QStringLiteral("#3498db")), // 蓝
            QColor(QStringLiteral("#e67e22")), // 橙
            QColor(QStringLiteral("#9b59b6")), // 紫
            QColor(QStringLiteral("#1abc9c")), // 青
            QColor(QStringLiteral("#e74c3c")), // 红
            QColor(QStringLiteral("#f1c40f")), // 黄
            QColor(QStringLiteral("#34495e")), // 深灰蓝
            QColor(QStringLiteral("#16a085")), // 深青
            QColor(QStringLiteral("#d35400")), // 深橙
            QColor(QStringLiteral("#8e44ad")), // 深紫
            QColor(QStringLiteral("#27ae60")), // 深绿
        };
    QList<QColor> color; // 扇区颜色列表
    if (count <= 0)
        return color;
    color.reserve(count); // 预分配列表容量
    for (int i = 0; i < count; ++i)//遍历扇区颜色列表
    {
        if (i < kBase.size()) // 扇区个数在预设范围之内
            color.append(kBase.at(i)); // 取当前固定调色板列表中的颜色插入到扇区颜色列表
        else // 扇区个数在预设范围之外
            color.append(QColor::fromHsv((i * 37) % 360, 200, 235)); // 按序生成颜色插入到扇区颜色列表
    }
    return color; // 返回扇区颜色列表
}
} // namespace

void SinglePieChart::setSectorNamesAndColors(const QStringList &names) // 设置扇区变量名与颜色
{
    m_sectorDataNames = names;
    m_sectorColors = sectorColorsForCount(names.size());
    update(); // 请求重绘
}

void SinglePieChart::applySectorDataCountsFromHash(const QHash<QString, unsigned int> &counts)// 根据存储饼图不同扇区对应数据变量名的列表的顺序，从哈希表中根据变量名取出对应的变量计数，组装成数据并加入饼图不同扇区对应数据的列表
{
    QList<VarCountData> next; // 存储饼图不同扇区对应数据的列表
    next.reserve(m_sectorDataNames.size()); // 预分配容量
    for (const QString &key : m_sectorDataNames) // 遍历存储饼图不同扇区对应数据变量名的列表
    {
        VarCountData row; // 创建数据对象
        row.name = key; // 数据变量名赋值为当前变量名
        row.count = counts.value(key, 0U); // 数据变量计数赋值为哈希表中变量名对应的值
        next.append(row);//插入数据
    }
    m_sectorData = next; // 更新饼图不同扇区对应数据的列表
    update(); // 请求重绘
}

// ---------- 尺寸提示 ----------

QSize SinglePieChart::minimumSizeHint() const // 返回饼图建议最小尺寸，避免布局将控件压得过小
{
    constexpr int kMinPieSide = 80; // 饼图外接正方形边长下限
    constexpr int kMinHeaderBarHeight = 22; // 标题栏高度下限
    constexpr int kMinRightContentWidth = 28; // 控件下方右侧的内容区域宽度下限
    const int mh = m_margins.left() + m_margins.right(); // 水平方向留白之和
    const int mv = m_margins.top() + m_margins.bottom(); // 垂直方向留白之和
    const QFontMetrics fm(font()); // fm对象拥有了当前字体的度量信息
    const int leg = m_sectorData.isEmpty() ? 0 : qMin(legendTheoreticalWidth(fm), 160); // 图例宽度，不超过160
    const int w = mh + leg + qMax(kMinRightContentWidth, kMinPieSide); // 控件宽度=水平留白 + 图例宽度 + 饼图宽度
    const int h = kMinHeaderBarHeight + mv + kMinPieSide; // 高度=标题栏高度 + 垂直留白 + 饼图高度
    return QSize(w, h); // 返回建议最小尺寸
}

QSize SinglePieChart::sizeHint() const // 返回饼图默认建议尺寸，供布局分配空间
{
    constexpr int kPreferPie = 140; // 饼图外接正方形边长
    const int mh = m_margins.left() + m_margins.right(); // 水平留白之和
    const int mv = m_margins.top() + m_margins.bottom(); // 垂直留白之和
    const QFontMetrics fm(font()); // 创建拥有当前字体的度量信息的字体度量对象,font()返回字体对象
    const int leg = m_sectorData.isEmpty() ? 0 : legendTheoreticalWidth(fm); // 将图例宽度初始化赋值为图例理论宽度
    const int w = qMax(220, mh + leg + kPreferPie); // 总宽度= max(220,水平留白+图例宽度+饼图外接正方形边长)
    int H = headerBarHeight(260) + mv + kPreferPie; // 总高度=标题栏高度(总高度的15%)+垂直留白+饼图外接正方形边长
    for (int i = 0; i < 3; ++i) // 标题栏高度依赖总高度,总高度又依赖标题栏高度，迭代直到收敛
    {
        const int hh = headerBarHeight(H); // 标题栏高度
        H = hh + mv + kPreferPie; // 总高度 = 标题栏高度 + 垂直留白 + 饼图外接正方形边长
    }
    return QSize(w, H); // 返回默认建议尺寸
}

// ---------- 控件上方：标题栏区域 ----------

void SinglePieChart::setPieChartOrdinal(int oneBasedIndex) // 设置标题栏中的饼图序号
{
    m_pieOrdinal = qMax(1, oneBasedIndex); // 饼图序号
    update(); // 请求重绘
}

int SinglePieChart::headerBarHeight(int widgetHeight) // 标题栏高度
{
    if (widgetHeight <= 0)
    {
        return 32;
    }
    return qBound(22, (widgetHeight * 15) / 100, 48); // 标题栏高度为控件高度的 15%，随控件高度变化而变化
}

QRect SinglePieChart::headerBarRect() const // 控件上方标题栏区域
{
    const int hh = headerBarHeight(height());// 标题栏高度
    const int hw = width();// 标题栏宽度
    if (hw <= 0 || hh <= 0)
    {
        return {};
    }
    return QRect(0, 0, hw, hh);
}

void SinglePieChart::drawHeaderBar(QPainter &p) const // 绘制标题栏
{
    const QRect hr = headerBarRect();// 标题栏区域
    if (hr.isEmpty())
    {
        return;
    }
    p.fillRect(hr, QColor(100, 100, 106)); // 标题栏深灰背景
    p.setPen(Qt::white);// 标题文字为白色
    QString title = QStringLiteral("饼图 %1").arg(m_pieOrdinal);
    if (m_sectorData.size() == 2)
    {
        title += QStringLiteral("  |  %1  %2")
                     .arg(sectorLabelText(m_sectorData.at(0)), sectorLabelText(m_sectorData.at(1)));
    }
    p.drawText(hr.adjusted(12, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, title);
}


// --------- 控件下方：扣除标题栏、并扣除留白后的内容区域----------

QRect SinglePieChart::bodyContentRect() const // 控件下方扣除标题栏、并扣除留白后的内容区域
{
    const int hh = headerBarHeight(height());// 标题栏高度
    return rect().adjusted(0, hh, 0, 0).marginsRemoved(m_margins);
}

namespace
{
QColor bodyContentBackgroundColor() // 控件下方扣除标题栏、并扣除留白后的内容区域背景颜色
{
    return QColor(52, 52, 56);
}
} // namespace

// ---------- 控件下方左侧：图例区域 ----------

int SinglePieChart::legendTheoreticalWidth(const QFontMetrics &fm) const // 图例理论宽度
{
    if (m_sectorData.isEmpty())
    {
        return 0;
    }
    constexpr int kSwatch = 10; // 正方形色块边长
    constexpr int kGap = 6; // 色块与文字间距
    int maxText = 0; // 最长文字宽度
    for (const VarCountData &v : std::as_const(m_sectorData)) // 遍历存储饼图不同扇区对应数据的列表
    {
        maxText = qMax(maxText, fm.horizontalAdvance(sectorLabelText(v))); // 更新最长文字宽度
    }
    return kSwatch + kGap + maxText; // 图例理论宽度=正方形色块边长+色块与文字间距+最长文字宽度
}

int SinglePieChart::legendReservedWidth() const // 图例实际宽度
{
    if (m_sectorData.isEmpty())
    {
        return 0;
    }
    const QRect br = bodyContentRect(); // 控件下方扣除标题栏、并扣除留白后的内容区域
    if (br.width() <= 0)
    {
        return 0;
    }
    const QFontMetrics fm(font()); // 创建拥有当前字体的度量信息的字体度量对象,font()返回字体对象
    int lrw = legendTheoreticalWidth(fm); // 将图例理论宽度初始化赋值为图例理论宽度
    constexpr int kMinRightContentWidth = 28; // 控件下方右侧的内容区域宽度下限
    lrw = qMin(lrw, qMax(0, br.width() - kMinRightContentWidth)); // 图例实际宽度=min(图例理论宽度,内容区域宽度减控件下方右侧的内容区域宽度下限)
    return lrw;
}

QRect SinglePieChart::leftContentRect() const // 控件下方左侧的图例区域
{
    const QRect br = bodyContentRect(); // 控件下方扣除标题栏、并扣除留白后的内容区域
    if (br.isEmpty())
    {
        return {};
    }
    const int lrw = legendReservedWidth(); // 图例实际宽度
    if (lrw <= 0)
    {
        return {};
    }
    return QRect(br.left(), br.top(), lrw, br.height());
}

void SinglePieChart::drawLeftLegend(QPainter &p) const // 绘制图例
{
    const QRect lr = leftContentRect(); // 控件下方左侧的图例区域
    if (m_sectorData.isEmpty() || lr.width() <= 0)
    {
        return;
    }
    const QFontMetrics fm(p.font()); // 创建拥有当前字体的度量信息的字体度量对象,font()返回字体对象
    constexpr int kSwatch = 10; // 正方形色块边长
    constexpr int kGap = 6; // 色块与文字间距
    constexpr int kRowPad = 4; // 行与行之间的竖向间距
    int y = lr.top(); // 一行图例的纵坐标,初始化赋值为图例区域顶边纵坐标
    for (int i = 0; i < m_sectorData.size(); ++i) // 遍历存储饼图不同扇区对应数据的列表
    {
        if (y + fm.height() > lr.bottom()) // 超出图例区域
        {
            break;
        }
        const int swY = y + (fm.height() - kSwatch) / 2; // 色块纵坐标，使色块垂直居中于文字行
        const QRect swR(lr.left(), swY, kSwatch, kSwatch); // 色块矩形
        const QColor col = (i < m_sectorColors.size()) ? m_sectorColors.at(i) : QColor(140, 140, 140); // 扇区色或默认灰
        p.setPen(Qt::NoPen); // 填充前不画描边
        p.setBrush(col); // 色块填充色
        p.drawRect(swR); // 填充色块
        p.setPen(QPen(QColor(200, 200, 200), 1)); // 色块外框浅灰细线
        p.setBrush(Qt::NoBrush); // 描边时不填充
        p.drawRect(swR); // 色块边框
        const int textW = lr.right() - (swR.right() + kGap) + 1; // 文字可用宽度
        if (textW > 0) 
        {
            p.setPen(Qt::white); // 图例文字白色
            const QString text = fm.elidedText(sectorLabelText(m_sectorData.at(i)), Qt::ElideRight, textW); // 名称与具体数量
            const QRect tr(swR.right() + kGap, y, textW, fm.height()); // 文字矩形
            p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, text); // 绘制文字
        }
        y += fm.height() + kRowPad; // 移到下一行
    }
}

// ---------- 控件下方右侧：饼图区域 ----------

QRect SinglePieChart::rightContentRect() const // 控件下方右侧的饼图区域
{
    const QRect br = bodyContentRect(); // 控件下方扣除标题栏、并扣除留白后的内容区域
    if (br.isEmpty())
    {
        return {};
    }
    const int lrw = legendReservedWidth(); // 左侧图例实际宽度
    const int rw = br.width() - lrw; // 右侧饼图宽度
    if (rw <= 0)
    {
        return {};
    }
    return QRect(br.left() + lrw, br.top(), rw, br.height());
}

QRect SinglePieChart::pieRect() const // 控件下方右侧的饼图区域内居中的正方形区域
{
    const QRect rr = rightContentRect(); // 控件下方右侧的饼图区域
    if (rr.width() <= 0 || rr.height() <= 0)
    {
        return {};
    }
    // 扇外引线先沿径向伸出约 12px，再折水平；顶/底扇区需在饼外留空，避免与 rightContentRect 求交后文字与箭头被裁没
    const QFontMetrics fm(font()); // 创建拥有当前字体的度量信息的字体度量对象,font()返回字体对象
    constexpr int kRadialStub = 12; // 扇区角平分线延长线长度
    const int idealPad = kRadialStub + (fm.height() + 1) / 3 + 3; // 单侧理想留白= 扇区角平分线延长线长度 + 水平段文字高度 + 小余量，防顶/底扇区扇外标注被裁
    const int maxSide = qMin(rr.width(), rr.height()); // 控件下方右侧的饼图区域可容纳的正方形最大边长
    const int reserve = qMin(idealPad, qMax(6, (maxSide - 40) / 2)); // 单侧实际留白
    const int side = qMax(0, maxSide - 2 * reserve); // 饼图外接正方形边长
    if (side <= 0)
    {
        return {}; 
    }
    const QPoint c = rr.center(); // 控件下方右侧的饼图区域中心点
    return QRect(c.x() - side / 2, c.y() - side / 2, side, side); // 控件下方右侧的饼图区域内居中的正方形区域
}

unsigned int SinglePieChart::totalCount() const // 总计数：各扇区对应数据的变量计数 count 之和
{
    unsigned int sum = 0;
    for (const VarCountData &v : std::as_const(m_sectorData)) // 遍历存储饼图不同扇区对应数据的列表
    {
        sum += v.count;
    }
    return sum;
}

QColor SinglePieChart::sectorColor(int index) const // 返回当前扇区对应的颜色
{
    if (index >= 0 && index < m_sectorColors.size())
    {
        return m_sectorColors.at(index);
    }
    return QColor(140, 140, 140); // 图例缺省色
}

void SinglePieChart::drawPieInnerSectorNames(QPainter &p, const QRect &pr, const QList<int> &start16, const QList<int> &span16) const// 绘制饼图内各个扇区对应变量的名称,传入参数(画家对象p，饼图外接正方形区域，扇区起始角列表，扇区跨越角列表)
{
    if (pr.isEmpty() || m_sectorData.isEmpty())
    {
        return;
    }
    const int n = m_sectorData.size(); // 扇区个数
    if (start16.size() != n || span16.size() != n)
    {
        return;
    }
    const QPointF center = pr.center(); // 饼图圆心（正方形 pieRect 的中心）
    const double R = pr.width() * 0.5; // 饼图半径
    if (R <= 0.0)
    {
        return;
    }
    const QFontMetrics fm(p.font()); // 创建拥有当前字体的度量信息的字体度量对象,font()返回字体对象
    p.setBrush(Qt::NoBrush); // 不填充背景
    p.setPen(Qt::white); // 扇内文字统一白色
    for (int i = 0; i < n; ++i) // 遍历存储饼图不同扇区对应数据的列表
    {
        const QString &name = m_sectorData.at(i).name; // 当前扇区对应数据的变量名
        if (name.isEmpty())
        {
            continue;
        }
        const QString innerText =
            QStringLiteral("%1\n%2").arg(name, QString::number(m_sectorData.at(i).count));
        const int mid16 = start16.at(i) + span16.at(i) / 2; // 扇区两条径向边夹角的角平分线方向
        const double rad = qDegreesToRadians(mid16 / 16.0); // 将角平分线方向转为弧度
        const QPointF dir(qCos(rad), -qSin(rad)); // 角平分线的单位方向向量；y 向下故 sin 取负，使 90° 对应屏幕正上方
        const QPointF anchor = center + (R * 0.5) * dir; // 文字锚点：沿角平分线在半径中点处，点 + 向量 = 移动后的新点
        QRect tr = fm.boundingRect(QRect(0, 0, static_cast<int>(R), static_cast<int>(R)), Qt::AlignCenter, innerText);
        tr.moveCenter(QPoint(qRound(anchor.x()), qRound(anchor.y())));
        p.drawText(tr, Qt::AlignCenter | Qt::TextWordWrap, innerText);
    }
}

void SinglePieChart::drawPieOuterSectorLabels(QPainter &p, const QRect &pr, const QList<int> &start16, const QList<int> &span16, unsigned int total) const // // 绘制饼图外各个扇区对应箭头指向的变量名称和所占百分比,传入参数(画家对象p，饼图外接正方形区域，扇区起始角列表，扇区跨越角列表，各扇区 count 之和)
{
    if (pr.isEmpty() || m_sectorData.isEmpty() || total == 0U)
    {
        return;
    }
    const int n = m_sectorData.size(); // 扇区个数
    if (start16.size() != n || span16.size() != n)
    {
        return;
    }
    const QRect rr = rightContentRect(); // 控件下方右侧的饼图区域
    const QPointF center = pr.center(); // 饼图圆心
    const double R = pr.width() * 0.5; // 饼图半径
    if (R <= 0.0)
    {
        return;
    }
    // 固定几何常量  p0：角平分线与饼图圆周交点。p1：p0按角平分线方向延伸线终点，为拐点。p2：p1按水平方向延伸线终点
    constexpr double kFixedLenP0P1 = 12.0; // p0—p1沿角平分线方向的延伸线长度
    constexpr double kFixedLenP1P2 = 44.0; // p1—p2沿水平方向的延伸线长度
    constexpr double kArrow = 5.0; // 从 p2 沿 p2→p1方向回退长度，用来绘制箭头V形两翼
    constexpr double kGap = 6.0; // p2 到文字近边水平间距
    constexpr double kColEps = 1e-3; // p0 与圆心同列判定容差
    //画家初始化
    const QFontMetrics fm(p.font()); // 创建拥有当前字体的度量信息的字体度量对象,font()返回字体对象
    p.save(); // 保存画家状态
    p.setBrush(Qt::NoBrush); // 不填充
    p.setPen(QPen(Qt::white, 1)); // 设置白色 1 像素线条
    for (int i = 0; i < n; ++i) // 遍历每个扇区
    {
        const int mid16 = start16.at(i) + span16.at(i) / 2; // 当前扇区角平分线方向角度
        const double rad = qDegreesToRadians(mid16 / 16.0); // 将扇区角平分线方向角度转为弧度
        const QPointF dir(qCos(rad), -qSin(rad)); // 当前扇区角平分线的单位方向向量
        const QPointF p0 = center + R * dir; // 角平分线与饼图圆周交点，点 + 向量 = 移动后的新点
        const QPointF p1 = p0 + kFixedLenP0P1 * dir; // p0按角平分线方向延伸线终点
        bool goLeft = false; // 判断水平段延伸方向（默认向右）
        const double dxCol = p0.x() - center.x();// 角平分线与饼图圆周交点与圆心的水平距离
        if (dxCol < -kColEps) // 交点在圆心左侧
        {
            goLeft = true; // 水平段向左
        }
        else if (dxCol > kColEps) // 交点在圆心右侧
        {
            goLeft = false; // 水平段向右
        }
        else // 南北方：p0 与圆心同列，不以 p0.x 相对圆心左右判定
        {
            if (dir.y() < 0.0) // 北向，平分线朝上
            {
                goLeft = false; // 规定北方向右
            }
            else if (dir.y() > 0.0) // 南向，平分线朝下
            {
                goLeft = true; // 规定南方向左
            }
            else //圆心重合
            {
                goLeft = (dir.x() < 0.0);
            }
        }
        const VarCountData &sector = m_sectorData.at(i);
        const QString dispName = sector.name.isEmpty() ? QStringLiteral("-") : sector.name;
        const double pct = 100.0 * static_cast<double>(sector.count) / static_cast<double>(total);
        const QString label = QStringLiteral("%1: %2 (%3%)")
                                  .arg(dispName)
                                  .arg(sector.count)
                                  .arg(pct, 0, 'f', 1);
        const int textW = fm.horizontalAdvance(label); // 标签宽度
        const int textH = fm.height(); // 标签(文字)高度
        const double yLine = p1.y(); // 水平延长线高度
        const double xMin = static_cast<double>(rr.left()) + 2.0; // 标签能够到达的最左极限位置
        const double xMax = static_cast<double>(rr.right()) - 2.0; // 标签能够到达的最右极限位置
        double p2x = 0.0; // 水平段终点p2的 x 坐标
        if (goLeft) // 水平段向左
        {
            p2x = qMin(p1.x(), qMax(xMin, p1.x() - kFixedLenP1P2));
        }
        else // 水平段向右
        {
            p2x = qMax(p1.x(), qMin(xMax, p1.x() + kFixedLenP1P2));
        }
        const QPointF p2(p2x, yLine); // 水平段终点p2
        const double actualHLen = qAbs(p2x - p1.x());
        if (actualHLen < 2.0) // 水平段过短无法布置箭头与文字
        {
            continue; // 跳过当前扇区
        }
        p.drawLine(p0, p1); // 绘制p0—p1沿角平分线方向的延伸线
        p.drawLine(p1, p2); // 绘制p1—p2沿水平方向的延伸线
        const QPointF horiz = p2 - p1; // 水平段向量,用于箭头方向
        const double hl = std::hypot(horiz.x(), horiz.y()); // 水平段长度
        if (hl > 1e-3) // 水平段大于判定容差时
        {
            const QPointF u(horiz.x() / hl, horiz.y() / hl); // 水平段单位向量
            const QPointF back = p2 - u * kArrow; // 箭头底部中心
            const QPointF n(-u.y(), u.x()); // 与水平段向量垂直的单位向量
            p.drawLine(back - n * 2.5, p2); // 箭头右翼
        }
        QRect tr(0, 0, textW, textH); // 标签区域
        if (goLeft) // 文字在 p2 左侧
        {
            tr.moveTopRight(QPoint(qRound(p2.x() - kGap), qRound(yLine - textH * 0.5))); // 矩形右上角对齐 p2 左偏
        }
        else // 文字在 p2 右侧
        {
            tr.moveTopLeft(QPoint(qRound(p2.x() + kGap), qRound(yLine - textH * 0.5))); // 矩形左上角对齐 p2 右偏
        }
        if (textW > 0 && textH > 0) 
        {
            p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, label); //绘制标签
        }
    }
    p.restore(); // 恢复 painter 状态
}

void SinglePieChart::drawRightPieChart(QPainter &p) const // 绘制饼图
{
    const QRect pr = pieRect(); // 控件下方右侧的饼图区域内居中的正方形区域
    if (pr.isEmpty())
    {
        return;
    }
    const unsigned int total = totalCount(); // 各扇区数量总和
    if (m_sectorData.isEmpty() || total == 0)//初始化占位圆
    {
        p.setPen(Qt::NoPen); // 占位圆不画描边
        p.setBrush(QColor(240, 240, 240)); // 浅灰填充
        p.drawEllipse(pr); // 画整圆占位
        return;
    }
    constexpr int kFullAngle = 360 * 16; // 整圆单位数，drawPie 单位为 1/16 度
    int remaining = kFullAngle; // 剩余配额单位数，分配给最后一个扇区，避免前面扇区舍入误差导致缝隙或重叠
    int angle = m_startAngle; // 当前扇区起始角
    p.setPen(QPen(Qt::white, 1)); // 扇区之间白色细线分隔
    const int n = m_sectorData.size(); // 扇区个数
    QList<int> start16; // 存储扇区起始角的列表
    QList<int> span16; // 存储扇区跨越角的列表
    start16.reserve(n); // 预分配容量
    span16.reserve(n); // 预分配容量
    for (int i = 0; i < n; ++i)// 遍历各个扇区
    {
        start16.append(angle); // 当前扇区起始角
        int span = 0; // 当前扇区跨越角
        if (i == n - 1)// 最后一个扇区
        {
            span = remaining; // 将剩余配额分配给最后一个扇区，避免前面扇区舍入误差导致缝隙或重叠
        }
        else
        {
            span = static_cast<int>(std::llround(static_cast<double>(kFullAngle) * (static_cast<double>(m_sectorData.at(i).count)/ static_cast<double>(total))));// 当前扇区跨越角度单位数=整圆单位数*当前扇区数据变量计数占总计数的比例，llround函数四舍五入取整
            if (span > remaining)// 如果需要分配的角度超过剩余配额
            {
                span = remaining; // 限制分配的角度为剩余配额
            }
            remaining -= span; // 更新剩余配额，扣除已分配的角度单位数
        }
        span16.append(span); // 当前扇区跨越角
        p.setBrush(sectorColor(i)); // 设置当前扇区填充色
        p.drawPie(pr, angle, span); // 绘制扇形
        angle += span; // 更新下一个扇区起始角
    }
    drawPieOuterSectorLabels(p, pr, start16, span16, total); // 绘制扇外引线、箭头与标签
    drawPieInnerSectorNames(p, pr, start16, span16); // 绘制扇内名称
}

// ---------- 绘制饼图：标题栏 → 左侧图例 → 右侧饼图 ----------

void SinglePieChart::paintEvent(QPaintEvent *event) // 按顺序绘制饼图：标题栏 → 左侧图例 → 右侧饼图
{
    Q_UNUSED(event); // 未使用事件参数，消除编译警告
    QPainter p(this); // 画家绑定到本控件画布
    p.setRenderHint(QPainter::Antialiasing, true); // 边缘抗锯齿
    const QRect r = rect(); // 整块控件区域
    if (!r.isEmpty())
    {
        p.fillRect(r, bodyContentBackgroundColor()); // 使用内容区底色填充整块控件区域
    }
    drawHeaderBar(p); // 绘制标题栏
    drawLeftLegend(p); // 绘制左侧图例
    drawRightPieChart(p); // 绘制右侧饼图
}
