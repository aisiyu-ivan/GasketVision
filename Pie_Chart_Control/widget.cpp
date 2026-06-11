#include "widget.h" 

#include "PieChartConfigDialog.h"
#include "SinglePieChart.h"

#include <QDialog>
#include <QDir> 
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QLayoutItem> 
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths> 
#include <QVBoxLayout>
#include <QtGlobal>

namespace
{
constexpr char kLastConfigFileKey[] = "config/lastFile";
} // namespace

// ---------- 窗口布局 ----------

Widget::Widget(QWidget *parent, const QString &settingsOrganization, const QString &settingsApplication,
               const QString &defaultConfigLeafName) //构造函数，主程序只传入父窗口指针，测试程序传入父窗口指针、组织名、应用名、文件名
    : QWidget(parent)
    , m_settingsOrg(settingsOrganization.isEmpty() ? QStringLiteral("Pie_Chart_Control") : settingsOrganization) // 组织名
    , m_settingsApp(settingsApplication.isEmpty() ? QStringLiteral("Pie_Chart_Control") : settingsApplication) // 应用名
    , m_defaultConfigLeaf(defaultConfigLeafName.isEmpty() ? QStringLiteral("pie_chart_layout.json") : defaultConfigLeafName) // 文件名
{
    setWindowTitle(QStringLiteral("Pie_Chart_Control")); // 窗口标题
    resize(960, 720); // 窗口尺寸

    auto *root = new QVBoxLayout(this); // 创建主窗口的垂直布局
    root->setContentsMargins(10, 10, 10, 10); // 布局与窗口边距
    root->setSpacing(8); // 顶栏与网格区域竖向间距

    // 顶栏配置按钮区域
    auto *top = new QHBoxLayout; // 创建一行水平布局(顶栏)
    top->addStretch(1); // 添加弹簧
    m_configButton = new QPushButton(QStringLiteral("配置")); // 创建配置按钮
    m_configButton->setMinimumWidth(88); // 按钮最小宽度
    connect(m_configButton, &QPushButton::clicked, this, &Widget::onOpenConfig); // 信号与槽机制，点击按钮打开配置对话框
    top->addWidget(m_configButton, 0, Qt::AlignRight); // 将按钮放入top指针指向的一行水平布局并右对齐
    root->addLayout(top); // 顶栏布局挂到主窗口的垂直布局,作为第一个垂直子项

    // 下方饼图网格区域
    m_gridHost = new QWidget; // 创建网格区域控件
    m_gridLayout = new QGridLayout(m_gridHost); // 在网格区域控件上创建网格布局
    m_gridLayout->setSpacing(12); // 网格间距
    m_gridLayout->setContentsMargins(4, 4, 4, 4); // 网格区内边距
    root->addWidget(m_gridHost, 1); // 网格区域控件挂到主窗口的垂直布局,作为第二个垂直子项

    loadPersistedConfigOrDefaults(); // 启动时加载配置
    ensureValidConfigAndCounts(); // 保证配置合法且变量名都有计数，否则使用演示配置并把变量计数补全为0
    rebuildGrid(); // 销毁旧饼图子控件并按配置重建
    updateWindowMinimumSize(); // 防止窗口过小导致饼图不可读
}

// ---------- 对外接口 ----------

void Widget::applyExternalVarsPerPieAndCounts(const QList<QStringList> &varsPerPie, const QHash<QString, unsigned int> &counts) // 外部按饼设置变量名列表与全局计数并重绘
{
    m_config.setVarsPerPie(varsPerPie); // 各饼一套 QStringList，条数与顺序与饼下标一致
    m_config.setVarCounts(counts); // 全局变量名→计数
    ensureValidConfigAndCounts(); // 保证配置合法且变量名都有计数，否则使用演示配置并把变量计数补全为0
    rebuildGrid(); // 销毁旧饼图子控件并按配置重建
    updateWindowMinimumSize(); // 防止窗口过小导致饼图不可读
}

PieChartConfig Widget::readConfiguration() const // 返回当前配置对象副本
{
    return m_config;
}

void Widget::initializeConfiguration(const PieChartConfig &configuration) // 配置参数的初始化写入
{
    m_config = configuration;
    ensureValidConfigAndCounts(); // 保证配置合法且变量名都有计数，否则使用演示配置并把变量计数补全为0
    rebuildGrid(); // 销毁旧饼图子控件并按配置重建
    updateWindowMinimumSize(); // 防止窗口过小导致饼图不可读
}

void Widget::applyExternalVarCountData(const std::vector<VarCountData> &data) // // 设置饼图数据，将vector转换为QHash，并写入配置对象
{
    QHash<QString, unsigned int> counts; // 存储变量名与变量计数的哈希表
    counts.reserve(static_cast<int>(data.size())); // 预分配容量
    for (const VarCountData &row : data) // 遍历存储饼图数据的vector
    {
        const QString n = row.name.trimmed(); // 去掉首尾空白
        if (n.isEmpty()) // 空变量名
            continue;
        counts.insert(n, row.count); // 将数据变量计数插入哈希表
    }
    m_config.setVarCounts(counts); // 全局变量名→计数
    ensureValidConfigAndCounts(); // 保证配置合法且变量名都有计数，否则使用演示配置并把变量计数补全为0
    pushDataToCharts(); // 把配置与计数同步到各饼图控件
}

// ---------- 槽函数：响应配置按钮，打开对话框 ----------

void Widget::onOpenConfig() // 槽函数：响应配置按钮，打开对话框
{
    PieChartConfigDialog dlg(m_config, this); // 创建配置对话框
    connect(&dlg, &PieChartConfigDialog::activeConfigFileChanged, this, [this](const QString &p) { setActivePersistPath(p); }); // 另存为后立即记住路径
    if (dlg.exec() != QDialog::Accepted) // 用户取消
        return;
    m_config = dlg.resultConfig(); // 用对话框结果覆盖内存配置
    if (m_config.isConfigContradictory()) 
    { // 若结果仍矛盾（防御）
        ensureValidConfigAndCounts(); // 保证配置合法且变量名都有计数，否则使用演示配置并把变量计数补全为0
        rebuildGrid(); // 销毁旧饼图子控件并按配置重建
        updateWindowMinimumSize(); // 防止窗口过小导致饼图不可读
        return; // 不保存非法配置到磁盘
    }
    ensureValidConfigAndCounts(); // 保证配置合法且变量名都有计数，否则使用演示配置并把变量计数补全为0
    rebuildGrid(); // 销毁旧饼图子控件并按配置重建
    updateWindowMinimumSize(); // 防止窗口过小导致饼图不可读
    saveConfigToFile(dlg.preferredPersistPath()); // 保存到「从文件加载」路径或当前活动路径
}

// ---------- 文件转换为配置对象 ----------

QString Widget::defaultConfigFilePath() const // 默认配置文件绝对路径
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); // 文件夹路径
    QDir().mkpath(base); // 确保路径存在
    return QDir(base).filePath(m_defaultConfigLeaf); // 拼接完整配置文件绝对路径：文件夹路径+文件名
}

QString Widget::activePersistPath() const // 读取上次使用的配置文件绝对路径
{
    QSettings s(m_settingsOrg, m_settingsApp); // 应用级设置键空间：组织名与应用名
    const QString stored = s.value(QString::fromUtf8(kLastConfigFileKey)).toString().trimmed(); // 读取已保存的路径
    if (!stored.isEmpty()) //路径非空
        return QFileInfo(stored).absoluteFilePath(); // 转换为绝对路径
    return defaultConfigFilePath(); // 无记录时返回默认路径
}

void Widget::setActivePersistPath(const QString &path) // 设置上次使用的配置文件绝对路径
{
    if (path.trimmed().isEmpty()) // 空路径
        return; 
    QSettings s(m_settingsOrg, m_settingsApp); // 应用级设置键空间
    s.setValue(QString::fromUtf8(kLastConfigFileKey), QFileInfo(path).absoluteFilePath()); // 存入 QSettings
}

void Widget::loadPersistedConfigOrDefaults() // 启动时加载文件转换为配置对象
{
    const QString last = activePersistPath(); // 上次使用的配置文件绝对路径
    if (QFile::exists(last) && m_config.loadFromFile(last) && !m_config.isConfigContradictory()) // 文件存在且能反序列化且与饼数等一致
        return;

    const QString def = defaultConfigFilePath(); // 默认配置文件绝对路径
    if (QFile::exists(def) && m_config.loadFromFile(def) && !m_config.isConfigContradictory()) // 默认文件
        return; 

    setDemoConfig(); // 无可用文件则写入演示用配置、变量计数
    setActivePersistPath(def); // 设置上次使用的配置文件绝对路径
    QFileInfo fi(def); // 围绕默认文件路径
    fi.dir().mkpath(QStringLiteral(".")); // 确保父目录存在再写入
    if (!m_config.saveToFile(def)) // 将演示配置落盘
        qWarning("Pie_Chart_Control: failed to write default config to %s", qPrintable(def)); // 落盘失败仅告警
}

// ---------- 配置对象转换为文件 ----------

void Widget::saveConfigToFile(const QString &dialogPreferredPath) // 设置配置对象转换为的文件路径
{
    QString path = dialogPreferredPath.trimmed(); // 去掉对话框/调用方传来的首尾空白
    if (path.isEmpty()) // 未指定文件时
        path = activePersistPath(); // 上次使用的配置文件绝对路径
    path = QFileInfo(path).absoluteFilePath(); // 转换为绝对路径
    QFileInfo fi(path); // 供检查父目录
    if (!fi.dir().exists()) // 目标父目录可能尚未创建
        fi.dir().mkpath(QStringLiteral(".")); // 递归创建目录
    if (!m_config.saveToFile(path)) // 将 m_config 序列化写入
    {
        QMessageBox::warning(this, QStringLiteral("配置"), QStringLiteral("无法保存配置文件：\n%1").arg(path)); // 向用户提示失败路径
        return; // 保存失败
    }
    setActivePersistPath(path); // 成功则记住本次写出的文件
}

// ---------- 演示用饼图 ----------

void Widget::setDemoConfig() // 设置演示用配置
{
    m_config.setVarCounts({}); // 清空演示切换前的变量计数，避免残留旧键与演示变量名不一致
    m_config.setGrid(3, 2); // 批量设置行数和列数为3行2列
    m_config.setPieCount(6); // 设置饼图个数为6个饼图
    m_config.setFillOrder(PieChartConfig::FillOrder::RowMajor); // 设置填充顺序为行优先
    const QStringList demoNames = {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")}; // 设置变量名列表
    for (int i = 0; i < m_config.pieCount(); ++i) // 遍历每个饼图
        m_config.setVarsForPie(i, demoNames); // 设置对应饼图的变量名列表
}

void Widget::setDemoCounts() // 设置演示用变量计数，并插入键值对容器
{
    QHash<QString, unsigned int> demo;
    demo.insert(QStringLiteral("A"), 400U); // 设置变量名A的计数为400
    demo.insert(QStringLiteral("B"), 250U); // 设置变量名B的计数为250
    demo.insert(QStringLiteral("C"), 100U); // 设置变量名C的计数为100
    m_config.setVarCounts(demo); // 整体写入配置对象的变量计数
}

void Widget::ensureValidConfigAndCounts() // 保证配置合法且变量名都有计数，否则使用演示配置并把变量计数补全为0
{
    if (m_config.isConfigContradictory()) // 配置不合法
        setDemoConfig(); // 设置演示用配置

    if (m_config.varCounts().isEmpty()) // 键值对容器为空
        setDemoCounts(); // 设置演示用变量计数，并插入键值对容器

    QHash<QString, unsigned int> next(m_config.varCounts()); // 复制当前变量计数，便于补全后一次性写回
    bool countsChanged = false; // 是否补全了缺失的变量名计数
    for (int i = 0; i < m_config.pieCount(); ++i) // 遍历每个饼图
    { 
        for (const QString &name : m_config.varsForPie(i))// 遍历当前饼图的变量名列表
        {
            if (!next.contains(name)) // 键值对容器中尚无该键
            {
                next.insert(name, 0U); // 插入键,并设置该键值为 0
                countsChanged = true;
            }
        }
    }
    if (countsChanged) // 若有补全
        m_config.setVarCounts(next); // 将补全后的变量计数写回配置对象
}

// ---------- 饼图重建 ----------

QPair<int, int> Widget::slotToGrid(int slotIndex) const// 将饼图下标换算为网格中的 (行, 列)
{
    if (slotIndex < 0 || slotIndex >= m_config.pieCount()) // 饼图下标不在有效范围内
        return QPair<int, int>(-1, -1);           // 返回无效坐标
    const int cols = m_config.cols();
    const int rows = m_config.rows();
    if (m_config.fillOrder() == PieChartConfig::FillOrder::RowMajor)// 行优先
    {
        const int row = slotIndex / cols;     // 行号 = 饼图下标整除列数
        const int col = slotIndex % cols;     // 列号 = 饼图下标对列数取余
        return QPair<int, int>(row, col);       // 返回 (行, 列) 供布局使用
    }
    else// 列优先
    {
        const int col = slotIndex / rows;         // 列号 = 饼图下标整除行数
        const int row = slotIndex % rows;         // 行号 = 饼图下标对行数取余
        return QPair<int, int>(row, col);           // 返回 (行, 列) 供布局使用
    }
}

void Widget::rebuildGrid() // 销毁旧饼图子控件并按配置重建
{
    while (QLayoutItem *it = m_gridLayout->takeAt(0)) // 从网格布局中移除的排在最前面的布局项
    { 
        if (QWidget *w = it->widget()) // 若该布局项是控件
            w->deleteLater(); // 异步销毁控件
        delete it; // 释放布局项
    }
    m_charts.clear(); // 清空存储指向各饼图指针的列表

    const int rows = m_config.rows(); // 网格行数
    const int cols = m_config.cols(); // 网格列数
    for (int r = 0; r < rows; ++r) // 遍历各行
        m_gridLayout->setRowStretch(r, 1); // 行拉伸因子
    for (int c = 0; c < cols; ++c) // 遍历各列
        m_gridLayout->setColumnStretch(c, 1); // 列拉伸因子

    const int n = m_config.pieCount(); // 饼图个数
    m_charts.reserve(n); // 为存储指向各饼图的指针的列表预留容量
    for (int i = 0; i < n; ++i)// 遍历每个饼图
     {
        auto *chart = new SinglePieChart(m_gridHost); // 创建单饼图，挂在网格区域父控件
        chart->setPieChartOrdinal(i + 1); //设置标题栏中的饼图序号
        const QPair<int, int> cell = slotToGrid(i); // 将当前饼图下标换算为网格中的 (行, 列)
        if (cell.first < 0) // 无效槽位
            continue;
        m_gridLayout->addWidget(chart, cell.first, cell.second); // 将当前饼图根据网格区域布局放入对应格子
        m_charts.append(chart); // 将当前饼图指针添加到存储指向各饼图指针的列表
    }
    pushDataToCharts(); // 把配置与计数同步到各饼图控件
}

void Widget::pushDataToCharts() // 把配置与计数同步到各饼图控件
{
    for (int i = 0; i < m_charts.size(); ++i) // 遍历存储指向各饼图的指针的列表
    { 
        SinglePieChart *chart = m_charts.at(i); //  指向当前饼图的指针
        chart->setSectorNamesAndColors(m_config.varsForPie(i)); // 设置扇区变量名与配色并请求重绘
        chart->applySectorDataCountsFromHash(m_config.varCounts()); // 根据存储饼图不同扇区对应的数据的变量名的列表的顺序，从哈希表中根据变量名key取出对应的变量计数value，组装成数据并加入饼图不同扇区对应的数据的列表，并请求重绘
    }
}

void Widget::updateWindowMinimumSize() // 防止窗口过小导致饼图不可读
{
    if (m_charts.isEmpty()) 
    { 
        setMinimumSize(480, 360); //设置窗口最小尺寸
        return;
    }
    const QSize cell = m_charts.first()->minimumSizeHint(); // 饼图建议最小尺寸
    const int w = m_config.cols() * cell.width() + (m_config.cols() - 1) * m_gridLayout->spacing() + 40; // 估算总宽
    const int h = m_config.rows() * cell.height() + (m_config.rows() - 1) * m_gridLayout->spacing() + 72; // 估算总高
    setMinimumSize(qMax(480, w), qMax(360, h)); // 与硬编码下限取较大值
}
