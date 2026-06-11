#ifndef PIECHARTCONFIG_H
#define PIECHARTCONFIG_H

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

class PieChartConfig// 饼图配置类
{

// ---------- 构造与生命周期 ----------

public:
    PieChartConfig();// 构造函数
    ~PieChartConfig();// 析构函数

// ---------- 规定饼图的行、列、数量的范围 ----------

private:
    int clampDim(int v) const;     // 限制行、列数量范围
    void clampPieCountToGrid();    // 限制饼图数量范围
    void syncVarsPerPieSize();     // 将列表大小调整为饼图数

// ---------- 读取配置对象数据 ----------

public:
    int rows() const { return m_rows; }               // 读取配置对象的行数
    int cols() const { return m_cols; }               // 读取配置对象的列数
    int pieCount() const { return m_pieCount; }         // 读取配置对象的饼图数量
    enum class FillOrder// 填充顺序枚举类
    {
        RowMajor,    // 行优先：先填满一行再换行
        ColumnMajor, // 列优先：先填满一列再换列
    };
    FillOrder fillOrder() const { return m_fillOrder; } // 读取配置对象的填充顺序
    QStringList varsForPie(int pieIndex) const; // 读取配置对象对应下标饼图的变量名列表
    const QHash<QString, unsigned int> &varCounts() const; // 读取配置对象的变量计数

// ---------- 设置配置对象数据 ----------

public:
    void setPieCount(int count);  // 设置配置对象的饼图个数
    void setGrid(int rows, int cols); // 批量设置配置对象的行数和列数
    void setFillOrder(FillOrder order); // 设置配置对象的填充顺序
    void setVarsForPie(int pieIndex, const QStringList &names); // 设置配置对象对应下标饼图的变量名列表
    void setVarsPerPie(const QList<QStringList> &lists); // 批量设置配置对象各个饼图的变量名列表
    void setVarCounts(const QHash<QString, unsigned int> &counts); // 设置配置对象的变量计数

// ---------- 判断配置是否合法 ----------

public:
    bool isConfigContradictory() const; // 判断配置是否合法

// ---------- 文件和配置对象的转换 ----------

public:
    QJsonObject toJson() const; // 配置对象 → 键值对容器(序列化)
    bool fromJson(const QJsonObject &obj); // 键值对容器 → 配置对象(反序列化)
    bool saveToFile(const QString &filePath) const; // 保存配置到文件
    bool loadFromFile(const QString &filePath); // 从文件读取配置

// ---------- 成员变量 ----------

private:
    static constexpr int kMinDim = 1;  // 行、列下限
    static constexpr int kMaxDim = 32; // 行、列上限
    int m_rows;                      // 行数
    int m_cols;                      // 列数
    int m_pieCount;                  // 饼图个数
    FillOrder m_fillOrder;           // 填充顺序枚举
    QList<QStringList> m_varsPerPie; // 存储各个饼图变量名列表的列表
    QHash<QString, unsigned int> m_varCounts; // 存储数据对应变量名与变量计数的键值对容器
};

#endif // PIECHARTCONFIG_H
