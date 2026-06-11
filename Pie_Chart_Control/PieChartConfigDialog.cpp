#include "PieChartConfigDialog.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QSet>
#include <algorithm>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

// ---------- 构造与界面搭建 ----------

PieChartConfigDialog::PieChartConfigDialog(const PieChartConfig &initial, QWidget *parent) // 构造函数,进行对话框布局
    : QDialog(parent) 
{
    m_targetPersistPath.clear(); // 

    setWindowTitle(QStringLiteral("饼图布局配置")); // 设置对话框标题栏文字
    resize(480, 420); // 设置对话框大小

    auto *root = new QVBoxLayout(this); // 创建对话框垂直布局

    // 对话框展示：另存为、从文件加载按钮
    auto *ioRow = new QHBoxLayout; // 按钮所在水平布局
    auto *btnSaveAs = new QPushButton(QStringLiteral("另存为")); // 创建另存为按钮
    auto *btnLoad = new QPushButton(QStringLiteral("从文件加载")); // 创建从文件加载按钮
    ioRow->addWidget(btnSaveAs); // 另存为按钮加入水平布局
    ioRow->addWidget(btnLoad); // 从文件加载按钮加入水平布局
    ioRow->addStretch(1); // 添加弹簧
    root->addLayout(ioRow); // 水平布局加入对话框垂直布局（最顶部）
    connect(btnSaveAs, &QPushButton::clicked, this, &PieChartConfigDialog::onSaveConfigAs); // 信号与槽连接：点击另存为按钮进行另存为操作
    connect(btnLoad, &QPushButton::clicked, this, &PieChartConfigDialog::onLoadConfigFromFile); // 信号与槽连接：点击从文件加载按钮进行从文件加载操作

    // 对话框展示：行、列、饼数、填充顺序
    auto *form = new QFormLayout; // 表单布局：左标签右控件
    m_rowsSpin = new QSpinBox; // 创建输入行数的数字输入框控件
    m_rowsSpin->setRange(1, 32); // 行数合法范围 
    m_colsSpin = new QSpinBox; // 创建输入列数的数字输入框控件
    m_colsSpin->setRange(1, 32); // 列数合法范围 
    m_pieCountSpin = new QSpinBox; // 创建输入饼图数量的数字输入框控件
    m_pieCountSpin->setRange(1, 32 * 32); // 饼图数量合法范围 
    m_fillOrderCombo = new QComboBox; // 创建填充顺序下拉框控件
    m_fillOrderCombo->addItem(QStringLiteral("行优先"), static_cast<int>(PieChartConfig::FillOrder::RowMajor)); // 在下拉框中添加行优先选项
    m_fillOrderCombo->addItem(QStringLiteral("列优先"), static_cast<int>(PieChartConfig::FillOrder::ColumnMajor)); // 在下拉框中添加列优先选项
    form->addRow(QStringLiteral("行数"), m_rowsSpin); // 在表单布局中添加行数的一行
    form->addRow(QStringLiteral("列数"), m_colsSpin); // 在表单布局中添加列数的一行
    form->addRow(QStringLiteral("饼图数量"), m_pieCountSpin); // 在表单布局中添加饼图数量的一行
    form->addRow(QStringLiteral("填充顺序"), m_fillOrderCombo); // 在表单布局中添加填充顺序的一行
    root->addLayout(form); // 将表单布局加入对话框垂直布局(顶部)

    // 对话框展示：变量名
    auto *varsGroup = new QGroupBox(QStringLiteral("各饼图变量名")); // 创建带标题的变量名分组框控件
    auto *varsOuter = new QVBoxLayout(varsGroup); // 创建分组框控件垂直布局
    m_scroll = new QScrollArea; // 创建容纳变量名编辑框控件的滚动容器控件
    m_scroll->setWidgetResizable(true); // 内容控件随视口宽度伸缩，高度超出则出现滚动条
    auto *scrollInner = new QWidget; // 滚动容器控件的内容控件
    m_varLinesLayout = new QVBoxLayout(scrollInner); // 创建内容控件垂直布局
    m_varLinesLayout->addStretch(); // 在内容控件垂直布局中添加弹簧
    m_scroll->setWidget(scrollInner); // 将内容控件挂到滚动容器控件
    varsOuter->addWidget(m_scroll); // 将滚动容器控件挂到分组框控件垂直布局
    root->addWidget(varsGroup, 1); // 将变量名分组框控件挂到对话框垂直布局(中部)

    // 对话框展示：变量计数
    auto *countsGroup = new QGroupBox(QStringLiteral("变量计数")); // 创建带标题的变量计数分组框控件
    auto *countsOuter = new QVBoxLayout(countsGroup);// 创建分组框控件垂直布局
    m_countScroll = new QScrollArea; // 创建容纳变量计数编辑框控件的滚动容器控件
    m_countScroll->setWidgetResizable(true); // 内容控件随视口宽度伸缩，高度超出则出现滚动条
    m_countInner = new QWidget; // 滚动容器控件的内容控件
    m_countsForm = new QFormLayout(m_countInner); // 在内容控件上创建表单布局：左变量名右变量计数编辑框
    m_countScroll->setWidget(m_countInner); // 将内容控件挂到滚动容器控件
    countsOuter->addWidget(m_countScroll); // 将滚动容器控件挂到分组框垂直布局
    root->addWidget(countsGroup, 0); // 将变量计数分组框挂到对话框垂直布局（底部）

    // 对话框展示：确定按钮
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok); // 创建对话框确定按钮控件
    root->addWidget(buttons); // 将按钮控件挂到对话框垂直布局(最底部)

    connect(buttons, &QDialogButtonBox::accepted, this, [this] { // 信号与槽连接：点击确定按钮校验配置,校验成功关闭对话框
        PieChartConfig trial; // 创建配置对象
        if (!applyUiToConfig(trial)) // 读取界面配置失败
         { 
            QMessageBox::warning(this, QStringLiteral("配置无效"), QStringLiteral("无法读取当前输入，请检查行、列与饼图数量。")); // 父窗口,标题,正文
            return; // 校验失败不关闭对话框
         }
        if (trial.isConfigContradictory())  //判断配置是否合法
        { 
            QMessageBox::warning(this, QStringLiteral("配置矛盾"),  QStringLiteral("请检查：饼图数量是否超过网格容量；每个饼内的变量名是否非空且不重复。")); // 父窗口,标题,正文
            return; // 校验失败不关闭对话框
        }
        accept(); // 校验成功,关闭对话框并返回 QDialog::Accepted,表示用户确认了在界面设置的配置
    });

    connect(m_rowsSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), // 信号与槽连接：输入行数的数字输入框控件内的行数改变，行数变化发出信号
            this, &PieChartConfigDialog::onGridDimsChanged); // 重新计算网格容量,更新数字输入框控件输入饼图数量的上限,保证饼图数量不超过网格容量

    connect(m_colsSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), // 信号与槽连接：输入列数的数字输入框控件内的列数改变，列数变化发出信号
            this, &PieChartConfigDialog::onGridDimsChanged); // 重新计算网格容量,更新数字输入框控件输入饼图数量的上限,保证饼图数量不超过网格容量

    connect(m_pieCountSpin, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), // 信号与槽连接：输入饼图数量的数字输入框控件输入饼图数量改变，饼图数量变化发出信号
            this, &PieChartConfigDialog::onPieCountChanged); // 重新计算网格容量,更新数字输入框控件输入饼图数量的上限,保证饼图数量不超过网格容量

    syncUiFromConfig(initial); // 将初始配置对象的配置数据写入配置对话框界面控件
}

// ---------- 配置对话框参数修改 ----------

void PieChartConfigDialog::onGridDimsChanged() // 维护网格容量与饼数一致：输入行数或列数的数字输入框控件内的行数或列数改变,重新计算网格容量,更新数字输入框控件输入饼图数量的上限,保证饼图数量不超过网格容量
{
    const int cap = qMax(1, m_rowsSpin->value() * m_colsSpin->value()); // 新网格容量
    m_pieCountSpin->setMaximum(cap); // 更新数字输入框控件输入饼图数量的上限
    if (m_pieCountSpin->value() > cap) // 若当前饼图数量超过新容量
        m_pieCountSpin->setValue(cap); // 自动下调到容量上限
}

void PieChartConfigDialog::onPieCountChanged(int value) // 维护饼数与变量名行数一致：输入饼图数量的数字输入框控件输入饼图数量改变，重建变量名编辑框控件与其对应的指针列表
{
    Q_UNUSED(value);
    rebuildVarLineEditors(); // 重建变量名编辑框控件与其对应的指针列表
    rebuildCountEditors(); // 重建变量计数编辑框控件与其对应的指针列表
}

void PieChartConfigDialog::rebuildVarLineEditors() // 重建变量名编辑框控件与其对应的指针列表
{
    while (QLayoutItem *it = m_varLinesLayout->takeAt(0)) // 从滚动容器控件上的内容控件上的垂直布局中移除的排在最前面的布局项
     { 
        if (it->widget()) // 若该项关联控件
            it->widget()->deleteLater(); // 异步销毁控件
        delete it; // 释放布局项本身
    }
    m_varLineEdits.clear(); // 清空存储指向变量名编辑框控件指针的列表

    const int n = m_pieCountSpin->value(); // 数字输入框控件输入饼图数量的值
    m_varLineEdits.reserve(n); // 为指针列表预留容量
    for (int i = 0; i < n; ++i) //遍历每个饼图
    { 
        auto *row = new QHBoxLayout; // 当前饼图的变量名编辑框所在的水平布局
        auto *lab = new QLabel(QStringLiteral("饼图 %1").arg(i + 1)); // 当前饼图的变量名编辑框的左侧文字标签控件
        auto *ed = new QLineEdit; // 当前饼图的变量名编辑框
        ed->setPlaceholderText(QStringLiteral("多个变量用逗号分隔，如：A,B,C,D")); // 占位提示
        row->addWidget(lab); // 将文字标签控件靠左添加到水平布局
        row->addWidget(ed, 1); // 将变量名编辑框靠右添加到水平布局
        auto *wrap = new QWidget; // 创建包裹变量名编辑框水平布局的控件
        wrap->setLayout(row); // 将水平布局设置到包裹控件上
        m_varLinesLayout->addWidget(wrap); // 将包裹控件添加到滚动容器控件上的内容控件上的垂直布局
        m_varLineEdits.append(ed); // 将当前饼图的变量名编辑框的指针添加到存储指向变量名编辑框控件指针的列表
        connect(ed, &QLineEdit::textChanged, this, &PieChartConfigDialog::onVarNamesTextChanged); // 变量名变化时刷新变量计数行集合
    }
    m_varLinesLayout->addStretch(); // 底部留白
}

namespace
{
QStringList splitNames(const QString &text) // 将变量名编辑框控件的文本转换为变量名列表
{
    QStringList out; // 最终输出的变量名列表
    const QStringList parts = text.split(QLatin1Char(',')); // 以逗号切分各变量名,得到变量名列表(带空格、有空串)
    out.reserve(parts.size()); // 变量名列表预分配容量
    for (QString p : parts) // 遍历变量名列表
    {
        p = p.trimmed(); // 变量名去掉首尾空白
        if (!p.isEmpty()) // 非空段才保留
            out.append(p); // 将有效变量名加入最终输出的变量名列表
    }
    return out;
}
} // namespace

namespace
{
static QStringList sortedUniqueNamesFromEdits(const QList<QLineEdit *> &edits) // 汇总各饼变量名编辑框控件得到所有数据对应的变量名列表（去重、排序）
{
    QSet<QString> s; // 存储所有不重复的变量名的集合
    for (const QLineEdit *ed : edits) //遍历存储指向所有变量名编辑框控件指针的列表
    {
        if (!ed)
            continue;
        for (const QString &p : splitNames(ed->text())) //遍历由当前变量名编辑框控件变成的变量名列表
        {
            const QString t = p.trimmed(); // 去首尾空格的变量名
            if (!t.isEmpty())
                s.insert(t); // 将当前变量名插入存储所有不重复的变量名的集合
        }
    }
    QStringList out = s.values(); // 将集合转为列表
    std::sort(out.begin(), out.end()); // 将列表按字典序升序排列
    return out;
}
} // namespace

void PieChartConfigDialog::onVarNamesTextChanged() // 槽函数：任一饼的变量名编辑框文本变化，重建变量计数编辑行以与当前变量名集合一致
{
    rebuildCountEditors();
}

void PieChartConfigDialog::rebuildCountEditors() // 重建变量计数编辑框控件与其对应的指针列表
{
    if (!m_countsForm) // 变量计数表单布局尚未创建
        return;

    QHash<QString, unsigned int> merged; // 汇总得到的变量名与将写回 m_dialogVarCounts 的计数的键值对容器
    const QStringList namesBefore = sortedUniqueNamesFromEdits(m_varLineEdits); // 汇总各饼变量名编辑框控件得到所有不重复变量名列表（已排序）
    for (const QString &n : namesBefore) //遍历上述变量名列表
    {
        unsigned v = m_dialogVarCounts.value(n, 0U); // 当前变量名在成员哈希中的计数
        if (m_nameToCountSpin.contains(n)) // 若已有该变量名对应的变量计数编辑框控件
            v = static_cast<unsigned int>(m_nameToCountSpin.value(n)->value()); // 以当前变量计数编辑框控件显示值为准
        merged.insert(n, v); // 将变量名与最终采用的计数写入汇总表
    }
    m_dialogVarCounts = merged; // 用汇总表整体替换成员哈希，避免后续删控件时丢失已编辑的计数

    while (m_countsForm->rowCount() > 0) // 只要变量计数表单布局里还有行
        m_countsForm->removeRow(0); // 每次删除第 0 行直至删光，旧 QSpinBox 随布局项销毁
    m_nameToCountSpin.clear(); // 清空存储变量名与指向变量计数编辑框控件指针的键值对容器

    const QStringList names = sortedUniqueNamesFromEdits(m_varLineEdits); // 再次从各饼变量名编辑框汇总变量名，与当前编辑框文本严格一致
    for (const QString &n : names) //遍历每个不重复变量名
    {
        auto *sp = new QSpinBox; // 当前变量名的变量计数编辑框控件
        sp->setRange(0, 2000000000); // 设置变量计数合法范围上限，避免 setValue 溢出
        const unsigned cur = m_dialogVarCounts.value(n, 0U); // 从成员哈希取该变量名的计数初值
        sp->setValue(static_cast<int>(qMin(cur, 2000000000U))); // 将初值钳在合法范围内后写入变量计数编辑框控件
        connect(sp, &QSpinBox::valueChanged, this, [this, n](int v) {
            m_dialogVarCounts.insert(n, static_cast<unsigned int>(v)); // 变量计数变化时写回成员哈希，供后续 rebuild 与 applyUiToConfig 读取
        });
        m_countsForm->addRow(n, sp); // 在变量计数表单布局中增加一行：左为变量名标签，右为变量计数编辑框控件
        m_nameToCountSpin.insert(n, sp); // 将变量名与指向该变量计数编辑框控件的指针存入键值对容器
    }
}

// ---------- 从界面读取并设置配置对象 ----------

bool PieChartConfigDialog::applyUiToConfig(PieChartConfig &out) const // 读取配置对话框界面，写入配置对象
{
    const int rows = m_rowsSpin->value(); // 数字输入框控件的网格行数
    const int cols = m_colsSpin->value(); // 数字输入框控件的网格列数
    const int pieCount = m_pieCountSpin->value(); // 数字输入框控件的饼图数量
    if (rows < 1 || cols < 1 || pieCount < 1)
        return false;
    out.setGrid(rows, cols); // 批量设置配置对象的行数和列数
    out.setPieCount(pieCount); // 设置配置对象的饼图个数
    const int fo = m_fillOrderCombo->currentData().toInt(); // 读取填充顺序下拉框控件当前选中项对应的的枚举整型
    out.setFillOrder(fo == static_cast<int>(PieChartConfig::FillOrder::ColumnMajor) ? PieChartConfig::FillOrder::ColumnMajor : PieChartConfig::FillOrder::RowMajor); // 设置配置对象的填充顺序
    QList<QStringList> lists; // 存储每个饼图对应变量名列表的列表
    lists.reserve(pieCount); // 为列表预分配空间
    for (int i = 0; i < pieCount; ++i) // 遍历每个饼图对应的变量名编辑框控件
    {
        if (i >= m_varLineEdits.size()) // 编辑行数不足
            return false; // 读取配置失败
        lists.append(splitNames(m_varLineEdits.at(i)->text())); // 将变量名编辑框控件的文本转换为变量名列表,并插入存储各个饼图变量名列表的列表
    }
    out.setVarsPerPie(lists); // 批量设置配置对象的各个饼图的变量名列表
    QSet<QString> uniq; // 存储所有不重复的变量名的集合
    for (const QStringList &sl : lists) //遍历存储每个饼图对应变量名列表的列表
    { 
        for (const QString &t : sl) //遍历当前饼图的变量名列表
        { 
            const QString x = t.trimmed(); // 变量名去掉首尾空白
            if (!x.isEmpty()) 
                uniq.insert(x); // 当前变量名插入集合去重
        }
    }
    QHash<QString, unsigned int> outCounts; // 变量计数键值对容器
    for (const QString &n : uniq) //遍历存储所有不重复的变量名的集合
     { 
        unsigned v = m_dialogVarCounts.value(n, 0U); // 键值对容器中当前变量名对应的变量计数
        if (m_nameToCountSpin.contains(n)) // 若界面仍有该变量计数编辑框
            v = static_cast<unsigned int>(m_nameToCountSpin.value(n)->value()); // 以界面当前值为准
        outCounts.insert(n, v); // 将变量名和对应变量计数加入变量计数键值对容器
    }
    out.setVarCounts(outCounts); // 将变量计数键值对容器写入配置对象
    return true; // 读取配置对话框界面，写入配置对象成功
}

PieChartConfig PieChartConfigDialog::resultConfig() const // 返回读取并写入配置对话框界面参数的配置对象
{
    PieChartConfig cfg; // 创建配置对象
    applyUiToConfig(cfg); // 读取界面配置,将配置写入配置对象
    return cfg;
}

// ---------- 配置对象写入界面 ----------

namespace
{
QString joinNames(const QStringList &names) // 将变量名列表拼成逗号分隔字符串,用于填入变量名编辑框控件
{
    return names.join(QStringLiteral(", ")); // 以逗号和空格连接各变量名
}
} // namespace

void PieChartConfigDialog::syncUiFromConfig(const PieChartConfig &cfg) // 将配置对象参数写入对话框：静态控件与 m_dialogVarCounts，再重建动态编辑区并写入各饼变量名
{
    m_dialogVarCounts = cfg.varCounts(); // 将配置对象的变量计数键值对容器赋值给编辑对话框时期的变量计数键值对容器
    // 阻塞信号，避免发出信号导致中间态
    QSignalBlocker brRows(m_rowsSpin); // 阻塞行数数字输入框控件变化发出信号
    QSignalBlocker brCols(m_colsSpin); // 阻塞列数数字输入框控件变化发出信号
    QSignalBlocker brPie(m_pieCountSpin); // 阻塞饼图数量数字输入框控件变化发出信号
    QSignalBlocker brFill(m_fillOrderCombo); // 阻塞填充顺序下拉框控件变化发出信号

    m_rowsSpin->setValue(cfg.rows()); // 设置行数数字输入框控件的值为配置对象的行数
    m_colsSpin->setValue(cfg.cols()); // 设置列数数字输入框控件的值为配置对象的列数
    const int cap = qMax(1, cfg.rows() * cfg.cols()); // 网格槽位容量
    m_pieCountSpin->setMaximum(cap); // 设置数字输入框控件输入饼图数量的上限为网格槽位容量
    m_pieCountSpin->setValue(qBound(1, cfg.pieCount(), cap)); // 设置数字输入框控件输入饼图数量的值为配置对象的饼图数量,范围限制在 [1, cap]

    const int fo = static_cast<int>(cfg.fillOrder()); // 将配置对象的填充顺序转换为整型
    const int idx = m_fillOrderCombo->findData(fo); // 在下拉框控件中查找配置对象的填充顺序对应的项的索引
    m_fillOrderCombo->setCurrentIndex(idx >= 0 ? idx : 0); // 设置下拉框控件的当前选中项为配置对象的填充顺序

    rebuildVarLineEditors(); // 界面重建：变量名行
    const int n = m_pieCountSpin->value(); // 数字输入框控件输入饼图数量的值
    for (int i = 0; i < n && i < m_varLineEdits.size(); ++i) // 将配置中各饼变量名列表写入对应变量名编辑框
        m_varLineEdits[i]->setText(joinNames(cfg.varsForPie(i))); // 将配置对象的各个饼图的变量名列表转换为逗号分隔字符串,并设置变量名编辑框控件的文本为该字符串
    rebuildCountEditors(); // 界面重建：变量计数行（依据上一步编辑框中的变量名集合）
}

// ---------- 另存为与从文件加载 ----------

QString PieChartConfigDialog::preferredPersistPath() const // 返回用户通过另存为或从文件加载指定的配置文件绝对路径
{
    return m_targetPersistPath;
}

void PieChartConfigDialog::onSaveConfigAs() // 另存为按钮：读取配置对话框界面写入临时配置对象并校验，打开另存为文件对话框选路径后将配置对象写入 JSON，更新 m_targetPersistPath 并发出 activeConfigFileChanged
{
    PieChartConfig trial; // 临时配置对象，仅用于校验与写盘，不替换主窗口中的 PieChartConfig
    if (!applyUiToConfig(trial)) // 读取配置对话框界面写入 trial 失败，例如行、列、饼图数量非法或变量名编辑框行数不足
    {
        QMessageBox::warning(this, QStringLiteral("配置无效"), QStringLiteral("无法读取当前输入。")); // 父窗口,标题,正文
        return;
    }
    if (trial.isConfigContradictory()) // 若临时配置对象行列表饼与变量名语义矛盾
    {
        QMessageBox::warning(this, QStringLiteral("配置矛盾"), QStringLiteral("请先修正行/列/饼数与变量名后再另存为。")); // 父窗口,标题,正文
        return;
    }
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); // 本应用可写配置目录路径，供另存为文件对话框默认目录
    QDir().mkpath(base); // 若目录不存在则创建，避免另存为文件对话框初始路径无效
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss")); // 日期时间字符串，用于建议文件名
    const QString suggested = QStringLiteral("pie_chart_%1.json").arg(stamp); // 另存为文件对话框中建议的默认文件名
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("另存为"), QDir(base).filePath(suggested),
                                                  QStringLiteral("JSON (*.json);;所有文件 (*.*)")); // 打开另存为文件对话框，返回用户所选路径或空串
    if (path.isEmpty()) // 用户在另存为文件对话框中取消
        return;
    QFileInfo fi(path); // 包装所选路径，便于取绝对路径与父目录
    fi.dir().mkpath(QStringLiteral(".")); // 若所选路径父目录不存在则创建，避免打开文件写入失败
    if (!trial.saveToFile(path)) // 将临时配置对象序列化为 JSON 并写入所选文件失败，例如无写权限或磁盘满
    {
        QMessageBox::warning(this, QStringLiteral("另存为"), QStringLiteral("写入失败：\n%1").arg(path)); // 父窗口,标题,正文
        return;
    }
    m_targetPersistPath = fi.absoluteFilePath(); // 记录另存为成功后的配置文件绝对路径，供 preferredPersistPath 返回给主窗口
    emit activeConfigFileChanged(m_targetPersistPath); // 通知主窗口将上次使用的配置文件路径更新为该绝对路径
    QMessageBox::information(this, QStringLiteral("另存为"), QStringLiteral("已保存：\n%1").arg(m_targetPersistPath)); // 父窗口,标题,正文
}

void PieChartConfigDialog::onLoadConfigFromFile() // 从文件加载按钮：打开文件对话框选择 JSON，读入为配置对象并校验合法后写入配置对话框界面，更新 m_targetPersistPath
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation); // 打开文件对话框的初始目录路径
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("从文件加载"), base,
                                                      QStringLiteral("JSON (*.json);;所有文件 (*.*)")); // 打开文件对话框，返回用户所选路径或空串
    if (path.isEmpty()) // 用户在打开文件对话框中取消
        return;
    PieChartConfig c; // 从磁盘读入的配置对象，成功后再同步到配置对话框界面
    if (!c.loadFromFile(path)) // 文件不存在、不可读或 JSON 与配置对象字段不兼容
    {
        QMessageBox::warning(this, QStringLiteral("从文件加载"), QStringLiteral("无法读取或解析该文件。")); // 父窗口,标题,正文
        return;
    }
    if (c.isConfigContradictory()) // 若读入的配置对象行列表饼与变量名语义矛盾，拒绝写入界面
    {
        QMessageBox::warning(this, QStringLiteral("从文件加载"), QStringLiteral("该文件配置不合法，未载入。")); // 父窗口,标题,正文
        return;
    }
    syncUiFromConfig(c); // 将读入的配置对象各字段写入静态控件，再重建变量名行与变量计数行并写入各饼变量名文本
    m_targetPersistPath = QFileInfo(path).absoluteFilePath(); // 记录本次加载的配置文件绝对路径，主窗口点确定后 saveConfigToFile 可优先写回该路径
}
