#include "PieChartConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtGlobal>

// ---------- 构造与生命周期 ----------

PieChartConfig::PieChartConfig()// 构造函数
    : m_rows(2)
    , m_cols(2)
    , m_pieCount(4)
    , m_fillOrder(FillOrder::RowMajor)
    , m_varsPerPie(4)
    , m_varCounts() // 初始化变量名到计数的映射为空
{
    syncVarsPerPieSize(); // 将列表大小调整为饼图数
}

PieChartConfig::~PieChartConfig() = default;// 析构函数

// ---------- 规定饼图的行、列、数量的范围 ----------

int PieChartConfig::clampDim(int v) const // 限制行、列数量范围
{
    return qBound(kMinDim, v, kMaxDim); // 将行、列数量限制在 [kMinDim, kMaxDim]
}

void PieChartConfig::clampPieCountToGrid() // 限制饼图数量范围
{
    const int cap = qMax(1, m_rows * m_cols); // 网格容量
    m_pieCount = qBound(1, m_pieCount, cap);   // 饼图数量范围在 [1, 网格容量]
}

void PieChartConfig::syncVarsPerPieSize() // 将列表大小调整为饼图数
{
    m_varsPerPie.resize(m_pieCount); // 列表大小调整为饼图数
}

// ---------- 读取配置对象数据 ----------

QStringList PieChartConfig::varsForPie(int pieIndex) const// 读取配置对象对应下标饼图的变量名列表
{
    if (pieIndex < 0 || pieIndex >= m_varsPerPie.size()) // 下标越界
        return {};
    return m_varsPerPie.at(pieIndex);
}


const QHash<QString, unsigned int> &PieChartConfig::varCounts() const // 读取配置对象的变量计数
{
    return m_varCounts;
}

// ---------- 设置配置对象数据 ----------

void PieChartConfig::setPieCount(int count)// 设置配置对象的饼图个数
{
    m_pieCount = count;          // 饼图个数赋值
    clampPieCountToGrid();       // 饼图数在范围内
    syncVarsPerPieSize();        // 列表大小调整为饼图数
}

void PieChartConfig::setGrid(int rows, int cols)// 批量设置配置对象的行数和列数
{
    m_rows = clampDim(rows);     // 行数范围内赋值
    m_cols = clampDim(cols);     // 列数范围内赋值
    clampPieCountToGrid();       // 饼图数在范围内
    syncVarsPerPieSize();        // 列表大小调整为饼图数
}

void PieChartConfig::setFillOrder(PieChartConfig::FillOrder order)// 设置配置对象的填充顺序
{
    m_fillOrder = order; // 填充顺序赋值
}

void PieChartConfig::setVarsForPie(int pieIndex, const QStringList &names)// 设置配置对象对应下标饼图的变量名列表
{
    syncVarsPerPieSize(); // 列表大小调整为饼图数
    if (pieIndex < 0 || pieIndex >= m_varsPerPie.size()) // 下标越界
        return;
    m_varsPerPie[pieIndex] = names; // 存储对应下标饼图的变量名列表赋值
}

void PieChartConfig::setVarsPerPie(const QList<QStringList> &lists)// 批量设置配置对象各个饼图的变量名列表
{
    syncVarsPerPieSize(); // 列表大小调整为饼图数
    for (int i = 0; i < m_pieCount; ++i) // 遍历存储饼图变量名列表的列表
        m_varsPerPie[i] = (i < lists.size()) ? lists.at(i) : QStringList(); //存储当前下标饼图的变量名列表赋值
}

void PieChartConfig::setVarCounts(const QHash<QString, unsigned int> &counts) // 设置配置对象的变量计数
{
    m_varCounts = counts; // 键值对容器赋值
}

// ---------- 判断配置是否合法 ----------

bool PieChartConfig::isConfigContradictory() const // 判断配置是否合法
{
    if (m_rows < kMinDim || m_rows > kMaxDim || m_cols < kMinDim || m_cols > kMaxDim) // 行或列落在合法范围外
        return true; // 不合法
    const int cap = m_rows * m_cols; // 网格槽位总数
    if (cap < 1) // 网格容量小于 1
        return true; // 不合法
    if (m_pieCount < 1 || m_pieCount > cap) // 饼图个数落在合法范围外
        return true; // 不合法
    if (m_varsPerPie.size() != m_pieCount) // 存储饼图变量名列表的列表的大小与当前饼图个数不一致
        return true; // 不合法
    for (int i = 0; i < m_pieCount; ++i)// 遍历存储饼图变量名列表的列表
    {
        const QStringList &names = m_varsPerPie.at(i); // 当前饼图对应的变量名列表
        QSet<QString> seen; // 哈希集合存储当前饼内已出现过的变量名
        seen.reserve(names.size()); // 按当前变量名列表元素个数为哈希集合预留容量
        for (const QString &n : names)// 遍历当前饼图的变量名列表
        {
            const QString t = n.trimmed(); // 去掉首尾空白，得到有效变量名
            if (t.isEmpty()) // 字符串全空白或空串不能作为合法变量名
                return true; // 不合法
            if (seen.contains(t)) // 当前饼内变量名重复
                return true; // 不合法
            seen.insert(t); // 将有效变量名插入存储当前饼图内已出现过的变量名的哈希集合
        }
    }
    return false; // 配置合法
}

// ---------- 文件和配置对象的转换 ----------

namespace // 匿名命名空间
{
int fillOrderToInt(PieChartConfig::FillOrder o) // 将填充顺序枚举转为整型,供 toJSON 使用,形参类型为类内枚举,需加作用域
{
    return static_cast<int>(o); // 将填充顺序枚举转为 int 返回
}

PieChartConfig::FillOrder fillOrderFromInt(int v) // 将整型还原为填充顺序枚举，供 fromJson 使用,返回值类型为类内枚举,需加作用域
{
    using FO = PieChartConfig::FillOrder; // 填充顺序枚举的别名
    return v == static_cast<int>(FO::ColumnMajor) ? FO::ColumnMajor : FO::RowMajor; // 判断整数 v 是否等于列优先对应的整型编码,若是返回列优先,否则返回行优先
}
} // namespace

QJsonObject PieChartConfig::toJson() const// 配置对象 → 键值对容器(序列化)
{
    QJsonObject obj; // 键值对容器
    obj.insert(QStringLiteral("rows"), m_rows); // 将行数插入键值对容器
    obj.insert(QStringLiteral("cols"), m_cols); // 将列数插入键值对容器
    obj.insert(QStringLiteral("pieCount"), m_pieCount); // 将饼图个数插入键值对容器
    obj.insert(QStringLiteral("fillOrder"), fillOrderToInt(m_fillOrder)); // 将填充顺序枚举的整型编码插入键值对容器
    QJsonArray pies; // 存储每个饼图变量名列表的数组
    for (const QStringList &names : m_varsPerPie)// 遍历存储饼图变量名列表的列表
    {
        QJsonArray namesArr; // 当前饼图的变量名数组
        for (const QString &n : names) // 遍历当前饼图变量名列表
        {
            namesArr.append(n); // 将当前变量名插入当前饼图的变量名数组
        }
        pies.append(namesArr); // 将当前饼图的变量名数组插入存储每个饼图变量名列表的数组
    }
    obj.insert(QStringLiteral("varsPerPie"), pies); // 将存储每个饼图变量名列表的数组插入键值对容器
    QJsonObject countsObj; // 存储数据对应变量名与变量计数的键值对容器
    for (auto it = m_varCounts.constBegin(); it != m_varCounts.constEnd(); ++it) // 遍历存储数据对应变量名与变量计数的键值对容器
        countsObj.insert(it.key(), static_cast<double>(it.value())); // 将当前数据对应的变量名与变量计数插入键值对容器
    obj.insert(QStringLiteral("varCounts"), countsObj); // 存储数据对应变量名与变量计数的键值对容器插入键值对容器
    return obj;
}

bool PieChartConfig::fromJson(const QJsonObject &obj)// 键值对容器 → 配置对象(反序列化)
{
    if (obj.isEmpty()) // 空对象
        return false; // 失败
    m_rows = clampDim(obj.value(QStringLiteral("rows")).toInt(m_rows)); // 行数
    m_cols = clampDim(obj.value(QStringLiteral("cols")).toInt(m_cols)); // 列数
    m_fillOrder = fillOrderFromInt(obj.value(QStringLiteral("fillOrder")).toInt(fillOrderToInt(m_fillOrder))); // 填充顺序
    m_pieCount = obj.value(QStringLiteral("pieCount")).toInt(m_pieCount); // 饼图个数
    clampPieCountToGrid(); // 限制饼图数量范围
    syncVarsPerPieSize(); // 列表大小调整为饼图数
    const QJsonValue vp = obj.value(QStringLiteral("varsPerPie")); // QJsonValue容器存储了各个饼图变量名列表
    if (vp.isArray())
    {
        const QJsonArray arr = vp.toArray(); // 将QJsonValue容器转换为数组,arr为存储不同饼图变量名列表的数组
        for (int i = 0; i < m_pieCount; ++i)// 遍历存储不同饼图变量名列表的数组QJsonArray
        {
            if (i < arr.size() && arr.at(i).isArray())
            {
                QStringList sl; // 当前饼图的变量名列表
                const QJsonArray na = arr.at(i).toArray(); // 将arr数组中的当前饼图的变量名列表转换为数组，na为当前饼图的变量名数组
                for (const QJsonValue &nv : na)// 遍历当前饼图的变量名数组
                {
                    if (nv.isString())
                        sl.append(nv.toString()); // 将na数组中的当前变量名转换为字符串，插入当前饼图的变量名列表
                }
                m_varsPerPie[i] = sl; // 将当前饼图的变量名列表赋值给存储饼图变量名列表的列表中对应位置的元素
            }
            else
            {
                m_varsPerPie[i] = QStringList(); // 置为空列表
            }
        }
    }
    m_varCounts.clear(); // 清空存储数据对应变量名与变量计数的键值对容器
    const QJsonValue cv = obj.value(QStringLiteral("varCounts")); // QJsonValue是存储数据对应变量名与变量计数的键值对容器的 JSON 值包装
    if (cv.isObject())
    {
        const QJsonObject co = cv.toObject(); // 将QJsonValue转换为键值对容器，co为存储数据对应变量名与变量计数的键值对容器
        for (auto it = co.constBegin(); it != co.constEnd(); ++it) // 遍历存储数据对应变量名与变量计数的键值对容器
        {
            const QString k = it.key(); // 当前的变量名
            const QJsonValue v = it.value(); // 当前的变量计数
            if (v.isDouble())
                m_varCounts.insert(k, static_cast<unsigned int>(v.toInt())); // 将当前键对应的值插入存储数据对应变量名与变量计数的键值对容器
            else if (v.isString())
                m_varCounts.insert(k, static_cast<unsigned int>(v.toString().toUInt())); // 将当前键对应的值插入存储数据对应变量名与变量计数的键值对容器
        }
    }
    return true; // 成功恢复配置对象
}

bool PieChartConfig::saveToFile(const QString &filePath) const// 保存配置到文件
{
    QFile f(filePath); // 文件对象
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) // 以截断写入方式打开
        return false;
    const QJsonDocument doc(toJson()); // 配置状态 → 键值对容器 → 文档
    if (f.write(doc.toJson(QJsonDocument::Indented)) < 0) // 文档 → 带缩进的 UTF-8字节数组 → 写入文件
        return false; // 写入失败
    return true; // 写入成功
}

bool PieChartConfig::loadFromFile(const QString &filePath)// 从文件读取配置
{
    QFile f(filePath); // 文件对象
    if (!f.open(QIODevice::ReadOnly)) // 只读打开
        return false; // 打开失败
    const QByteArray data = f.readAll(); // 将读取全部内容赋值给字节数组
    QJsonParseError err{}; // 解析错误信息
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err); // 字节数组 → 文档，并把解析错误信息交给 err
    if (err.error != QJsonParseError::NoError || !doc.isObject()) // 语法错误或根不是对象
        return false; // 失败
    return fromJson(doc.object()); // 文档 → 键值对容器 → 配置状态，返回 fromJson 结果
}
