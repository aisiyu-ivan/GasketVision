#include "InspectionWindow.h"

#include "InspectionDataService.h"

#include <QCoreApplication>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QtGlobal>
#include <QTimer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
#ifdef GASKETVISION_HMI_TEST
constexpr bool kTestHmi = true;
#else
constexpr bool kTestHmi = false;
#endif

constexpr int kPipelineStaggerMs = 2000;
constexpr int kWaitForExternalConnectMs = 500;

// 设置控件背景色
void setWidgetBackground(QWidget *widget, const QColor &color)
{
    widget->setAutoFillBackground(true);
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, color);
    widget->setPalette(palette);
}
} // namespace

// 构建界面布局、初始化 OK/NG 面板并启动 TCP 数据服务
InspectionWindow::InspectionWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1280, 720);

    setWidgetBackground(this, QColor(0x25, 0x25, 0x25));

    auto *central = new QWidget(this);
    setWidgetBackground(central, Qt::white);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 12);
    root->setSpacing(0);

    m_headerBar = new QWidget(central);
    m_headerBar->setObjectName(QStringLiteral("headerBar"));
    m_headerBar->setFixedHeight(52);
    m_headerBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_headerBar->setStyleSheet(QStringLiteral(
        "#headerBar{background:#ffffff;}"
        "QPushButton{"
        "  background-color:#f5f5f5;color:#222222;"
        "  border:1px solid #cccccc;border-radius:4px;padding:5px 12px;"
        "}"
        "QPushButton:hover{background-color:#ebebeb;}"
        "QPushButton:pressed{background-color:#dddddd;}"
        "QPushButton:checked{background-color:#dceefb;border-color:#3498db;color:#222222;}"));

    auto *headerGrid = new QGridLayout(m_headerBar);
    headerGrid->setContentsMargins(12, 6, 12, 6);
    headerGrid->setHorizontalSpacing(8);
    headerGrid->setColumnStretch(0, 1);
    headerGrid->setColumnStretch(1, 0);
    headerGrid->setColumnStretch(2, 1);

    m_btnLoadImage = new QPushButton(QStringLiteral("从文件加载图片"));
    m_btnSaveImage = new QPushButton(QStringLiteral("另存为图片"));
    m_btnSavePieData = new QPushButton(QStringLiteral("另存为统计数据"));
    m_resultLabel = new QLabel(QStringLiteral("状态:"));
    QFont resultFont = m_resultLabel->font();
    resultFont.setPointSize(16);
    resultFont.setBold(true);
    m_resultLabel->setFont(resultFont);
    m_resultLabel->setStyleSheet(QStringLiteral("color:#222222;background:transparent;"));
    m_modeLabel = new QLabel(modeDisplayName());
    QFont modeFont = m_modeLabel->font();
    modeFont.setPointSize(14);
    modeFont.setBold(true);
    m_modeLabel->setFont(modeFont);
    m_modeLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel = new QLabel;
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(14);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#555555;background:transparent;"));
    setStatusMessage(QStringLiteral("正在初始化…"));

    auto *btnWidget = new QWidget(m_headerBar);
    btnWidget->setStyleSheet(QStringLiteral("background:transparent;"));
    auto *btnLayout = new QHBoxLayout(btnWidget);
    btnLayout->setContentsMargins(0, 0, 0, 0);
    btnLayout->setSpacing(6);
    btnLayout->addWidget(m_btnLoadImage);
    btnLayout->addWidget(m_btnSaveImage);
    btnLayout->addWidget(m_btnSavePieData);

    auto *statusWidget = new QWidget(m_headerBar);
    statusWidget->setStyleSheet(QStringLiteral("background:transparent;"));
    auto *statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(4);
    statusLayout->addStretch(1);
    statusLayout->addWidget(m_resultLabel);
    statusLayout->addWidget(m_statusLabel);

    headerGrid->addWidget(btnWidget, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    headerGrid->addWidget(m_modeLabel, 0, 1, Qt::AlignCenter);
    headerGrid->addWidget(statusWidget, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    root->addWidget(m_headerBar, 0);

    auto *headerLine = new QFrame(central);
    headerLine->setFixedHeight(1);
    headerLine->setFrameShape(QFrame::NoFrame);
    setWidgetBackground(headerLine, QColor(0xe0, 0xe0, 0xe0));
    root->addWidget(headerLine);

    auto *bodyWidget = new QWidget(central);
    setWidgetBackground(bodyWidget, Qt::white);
    bodyWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *body = new QHBoxLayout(bodyWidget);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    auto *imagePanel = new QWidget(bodyWidget);
    imagePanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setWidgetBackground(imagePanel, Qt::black);
    auto *imageLayout = new QVBoxLayout(imagePanel);
    imageLayout->setContentsMargins(0, 0, 0, 0);
    imageLayout->setSpacing(0);

    m_imageLabel = new QLabel(imagePanel);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setMinimumSize(400, 300);
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_imageLabel->setStyleSheet(QStringLiteral("background: transparent; color: #aaa;"));
    m_imageLabel->setText(QStringLiteral("等待检测图像…"));

    m_shmImageLabel = new ShmImageLabel(imagePanel);
    m_shmImageLabel->hide();

    imageLayout->addWidget(m_imageLabel);
    imageLayout->addWidget(m_shmImageLabel);

    m_ipcPaintFallbackTimer = new QTimer(this);
    m_ipcPaintFallbackTimer->setSingleShot(true);
    m_ipcPaintFallbackTimer->setInterval(2000);
    connect(m_ipcPaintFallbackTimer, &QTimer::timeout, this, &InspectionWindow::onIpcPaintFallback);
    connect(m_shmImageLabel, &ShmImageLabel::framePainted, this, &InspectionWindow::onIpcFramePainted);

    auto *gutter = new QWidget(bodyWidget);
    gutter->setFixedWidth(16);
    setWidgetBackground(gutter, Qt::white);

    auto *statsPanel = new QWidget(bodyWidget);
    statsPanel->setMinimumWidth(280);
    statsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setWidgetBackground(statsPanel, QColor(0x2f, 0x2f, 0x2f));
    auto *statsLayout = new QVBoxLayout(statsPanel);
    statsLayout->setContentsMargins(0, 0, 0, 0);
    statsLayout->setSpacing(0);

    m_statsPanel = new OkNgStatsPanel(statsPanel);
    m_statsPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    statsLayout->addWidget(m_statsPanel);

    body->addWidget(imagePanel, 7);
    body->addWidget(gutter);
    body->addWidget(statsPanel, 3);
    root->addWidget(bodyWidget, 1);

    setCentralWidget(central);

    connect(m_btnLoadImage, &QPushButton::clicked, this, &InspectionWindow::onLoadImage);
    connect(m_btnSaveImage, &QPushButton::clicked, this, &InspectionWindow::onSaveImage);
    connect(m_btnSavePieData, &QPushButton::clicked, this, &InspectionWindow::onSavePieData);

    initStatsPanel();
    initProfile();
    m_dataService = new InspectionDataService(this, this);
    connect(m_dataService, &InspectionDataService::statusMessage, this, &InspectionWindow::setStatusMessage);
    connect(m_dataService, &InspectionDataService::statusSocketListening, this, [this]() {
        m_pipelineReady = true;
        onControlListenReady();
    });
    m_dataService->startCommunication();
}

// 窗口尺寸变化时重绘图像
void InspectionWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_ipcDisplayActive)
        m_shmImageLabel->update();
    else
        updateImageView();
}

// 首次显示时刷新模式标签样式
void InspectionWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    updateModeLabelStyle();
}

// 析构主窗口（数据服务随 QObject 树自动释放）
InspectionWindow::~InspectionWindow() = default;

// 加载 OK/NG 计数配置
void InspectionWindow::initStatsPanel()
{
    const QString defaultPath = QStringLiteral("pie_chart_okng.json");
    if (!m_statsPanel->loadFromFile(defaultPath))
    {
        QHash<QString, unsigned int> counts;
        counts.insert(QStringLiteral("OK"), 0U);
        counts.insert(QStringLiteral("NG"), 0U);
        m_statsPanel->setCounts(counts);
    }
}

// 根据检测结果更新图像与 OK/NG 状态标签
void InspectionWindow::applyInspectionResult(const InspectionResult &result)
{
    if (result.fromIpc)
    {
        if (result.annotatedImage.isNull())
        {
            setStatusMessage(QStringLiteral("收到 IPC 帧但标注图为空（检查 Engine 是否已标注发布）"));
            return;
        }

        m_ipcDisplayActive = true;
        m_ipcAnnotatedView = result.annotatedImage;
        m_pendingIpcReleaseFrameId = result.frameId;
        m_imageLabel->hide();
        m_shmImageLabel->show();
        m_shmImageLabel->setShmImageView(m_ipcAnnotatedView, result.frameId);
        m_ipcPaintFallbackTimer->start();

        QString saveError;
        if (!saveIpcImagesToDisk(result, &saveError))
            setStatusMessage(QStringLiteral("图像已显示，落盘失败：%1").arg(saveError));
    }
    else
    {
        m_ipcDisplayActive = false;
        m_pendingIpcReleaseFrameId = 0;
        m_ipcPaintFallbackTimer->stop();
        m_shmImageLabel->clearShmView();
        m_shmImageLabel->hide();
        m_imageLabel->show();

        const QString annotated = result.annotatedImagePath.isEmpty() ? result.imagePath : result.annotatedImagePath;
        if (!annotated.isEmpty())
        {
            m_annotatedPath = resolveImagePath(annotated);
            m_annotatedPixmap = loadPixmap(m_annotatedPath);
        }
        if (m_annotatedPixmap.isNull())
            return;
        updateImageView();
    }

    m_lastOkNg = result.ok ? QStringLiteral("OK") : QStringLiteral("NG");
    m_statusLabel->setText(m_lastOkNg);
    m_statusLabel->setStyleSheet(result.ok ? QStringLiteral("color:#2ecc71;background:transparent;")
                                           : QStringLiteral("color:#e74c3c;background:transparent;"));
}

bool InspectionWindow::saveIpcImagesToDisk(const InspectionResult &result, QString *errorOut)
{
    const QString baseDir = m_activeWorkDir.isEmpty() ? QDir::currentPath() : m_activeWorkDir;
    const QString capturesDir = QDir(baseDir).filePath(QStringLiteral("captures"));
    if (!QDir().mkpath(capturesDir))
    {
        if (errorOut)
            *errorOut = capturesDir;
        return false;
    }

    const QString annotRel = result.annotatedImagePath.isEmpty()
                                 ? QStringLiteral("captures/%1_annot.png").arg(result.frameId)
                                 : result.annotatedImagePath;
    const QString annotPath = resolveImagePath(annotRel);

    QFileInfo annotFi(annotPath);
    annotFi.dir().mkpath(QStringLiteral("."));

    if (!result.annotatedImage.save(annotPath))
    {
        if (errorOut)
            *errorOut = annotPath;
        return false;
    }

    m_annotatedPath = annotPath;
    return true;
}

void InspectionWindow::onIpcFramePainted(quint64 frameId)
{
    if (frameId == 0 || frameId != m_pendingIpcReleaseFrameId)
        return;
    m_ipcPaintFallbackTimer->stop();
    m_pendingIpcReleaseFrameId = 0;
    emit ipcDisplayFinished(frameId);
}

void InspectionWindow::onIpcPaintFallback()
{
    if (m_pendingIpcReleaseFrameId == 0)
        return;
    const quint64 frameId = m_pendingIpcReleaseFrameId;
    m_pendingIpcReleaseFrameId = 0;
    emit ipcDisplayFinished(frameId);
}

// 按编译 profile 固定工作目录与窗口标题
void InspectionWindow::initProfile()
{
    if constexpr (kTestHmi)
    {
        setWindowTitle(QStringLiteral("工业垫片视觉检测系统 — 测试"));
        m_activeWorkDir = resolveTestWorkDir();
        if (m_activeWorkDir.isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("测试系统"),
                                 QStringLiteral("未找到 test/vision_engine.json，请从 test/ 目录启动 HMI_Test。"));
        }
        else
        {
            QString assetError;
            if (!validateTestAssets(m_activeWorkDir, &assetError))
                QMessageBox::warning(this, QStringLiteral("测试系统"), assetError);
        }
    }
    else
    {
        setWindowTitle(QStringLiteral("工业垫片视觉检测系统"));
        m_activeWorkDir = resolveCameraWorkDir();
        if (m_activeWorkDir.isEmpty())
        {
            QMessageBox::warning(this, QStringLiteral("正式系统"),
                                 QStringLiteral("未找到 vision_engine.json，请从 build/ 目录启动 HMI。"));
        }
    }

    updateModeLabelStyle();
}

// 校验测试目录下样品图与定位模板
bool InspectionWindow::validateTestAssets(const QString &workDir, QString *errorOut) const
{
    const QDir stationDir(QDir(workDir).filePath(QStringLiteral("station1")));
    if (!stationDir.exists())
    {
        if (errorOut)
            *errorOut = QStringLiteral("缺少 test/station1/ 目录，请先运行 test/run_test.bat。");
        return false;
    }

    const QStringList images = stationDir.entryList({QStringLiteral("*.png"), QStringLiteral("*.jpg"),
                                                   QStringLiteral("*.bmp")},
                                                  QDir::Files);
    if (images.isEmpty())
    {
        if (errorOut)
            *errorOut = QStringLiteral("test/station1/ 中没有样品图，请先运行 test/run_test.bat。");
        return false;
    }

    const QString templatePath = QDir(workDir).filePath(QStringLiteral("templates/fiducial_L.png"));
    if (!QFileInfo::exists(templatePath))
    {
        if (errorOut)
            *errorOut = QStringLiteral("缺少 test/templates/fiducial_L.png，请先运行 test/run_test.bat。");
        return false;
    }

    return true;
}

// 结束可能残留的 VisionEngine / CameraService
void InspectionWindow::stopBackendProcesses()
{
#ifdef Q_OS_WIN
    auto killIfRunning = [](const char *imageName) {
        QProcess proc;
        proc.setProgram(QStringLiteral("taskkill"));
        proc.setArguments({QStringLiteral("/F"), QStringLiteral("/IM"), QString::fromLatin1(imageName)});
        proc.start();
        proc.waitForFinished(2000);
    };
    killIfRunning("VisionEngine.exe");
    killIfRunning("CameraService.exe");
#endif
}

// 分离启动 VisionEngine 与 CameraService（非父子进程，等同 run.bat 的 start）
bool InspectionWindow::launchDetachedPipeline()
{
    if (m_pipelineLaunchAttempted)
        return true;
    m_pipelineLaunchAttempted = true;

    stopBackendProcesses();

    if (m_dataService)
        m_dataService->resetIpcConnection();

    const QString exeDir = QCoreApplication::applicationDirPath();
    QString workDir = m_activeWorkDir;
    if (workDir.isEmpty())
        workDir = kTestHmi ? resolveTestWorkDir() : resolveCameraWorkDir();
    if (workDir.isEmpty())
    {
        setStatusMessage(QStringLiteral("未找到 vision_engine.json，无法启动检测链路"));
        return false;
    }

    const QString engineExe = QDir(exeDir).filePath(QStringLiteral("VisionEngine.exe"));
    const QString cameraExe = QDir(exeDir).filePath(QStringLiteral("CameraService.exe"));
    if (!QFileInfo::exists(engineExe) || !QFileInfo::exists(cameraExe))
    {
        setStatusMessage(QStringLiteral("未找到 VisionEngine.exe 或 CameraService.exe（与 HMI 同目录）"));
        return false;
    }

    const QStringList args{QStringLiteral("vision_engine.json")};

    if (!QProcess::startDetached(engineExe, args, workDir))
    {
        setStatusMessage(QStringLiteral("启动 VisionEngine 失败"));
        return false;
    }

    QTimer::singleShot(500, this, [this, cameraExe, workDir, args]() {
        if (!QProcess::startDetached(cameraExe, args, workDir))
            setStatusMessage(QStringLiteral("启动 CameraService 失败"));
        else
            setStatusMessage(kTestHmi ? QStringLiteral("检测链路已就绪，等待合成样品…")
                                      : QStringLiteral("检测链路已就绪，等待相机采图…"));
    });

    setStatusMessage(QStringLiteral("正在启动 VisionEngine 与 CameraService…"));
    return true;
}

// 本地套接字就绪后分离启动检测链路
void InspectionWindow::onControlListenReady()
{
    if (m_dataService)
        m_dataService->clearSession();
    QTimer::singleShot(kWaitForExternalConnectMs, this, [this]() {
        launchDetachedPipeline();
    });
}

// 当前 profile 对应的模式显示名
QString InspectionWindow::modeDisplayName() const
{
    return kTestHmi ? QStringLiteral("测试模式") : QStringLiteral("相机模式");
}

// 更新状态栏灰色提示文字
void InspectionWindow::setStatusMessage(const QString &text)
{
    if (!m_statusLabel)
        return;
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#555555;background:transparent;"));
}

// 刷新顶部「相机模式 / 测试模式」标签颜色与文字
void InspectionWindow::updateModeLabelStyle()
{
    if (!m_modeLabel)
        return;
    const QString color = kTestHmi ? QStringLiteral("#3498db") : QStringLiteral("#e67e22");
    m_modeLabel->setStyleSheet(QStringLiteral("color:%1;background:transparent;").arg(color));
    m_modeLabel->setText(modeDisplayName());
}

// 解析正式环境工作目录（含 vision_engine.json）
QString InspectionWindow::resolveCameraWorkDir() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appCfg = QDir(appDir).filePath(QStringLiteral("vision_engine.json"));
    if (QFileInfo::exists(appCfg))
        return appDir;

    return resolveWorkDir();
}

// 解析测试环境工作目录（test/vision_engine.json）
QString InspectionWindow::resolveTestWorkDir() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString projectRoot = QFileInfo(appDir).dir().absolutePath();
    const QString testDir = QDir(projectRoot).filePath(QStringLiteral("test"));
    const QString testCfg = QDir(testDir).filePath(QStringLiteral("vision_engine.json"));
    if (QFileInfo::exists(testCfg))
        return testDir;

    const QString cwdCfg = QDir(QDir::currentPath()).filePath(QStringLiteral("vision_engine.json"));
    if (QFileInfo::exists(cwdCfg))
    {
        QFile file(cwdCfg);
        if (file.open(QIODevice::ReadOnly))
        {
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.object().value(QStringLiteral("imageSource")).toString().toLower() == QStringLiteral("synthetic"))
                return QDir::currentPath();
        }
    }
    return {};
}

// 更新 OK/NG 计数面板
void InspectionWindow::setPieCounts(const QHash<QString, unsigned int> &counts)
{
    m_statsPanel->setCounts(counts);
}

// 在多个基准目录中解析图像相对路径
QString InspectionWindow::resolveImagePath(const QString &path) const
{
    QFileInfo fi(path);
    if (fi.isAbsolute() && fi.exists())
        return fi.absoluteFilePath();

    const QStringList bases = {
        QDir::currentPath(),
        QCoreApplication::applicationDirPath(),
        QFileInfo(QCoreApplication::applicationDirPath()).dir().absolutePath(),
    };
    for (const QString &base : bases)
    {
        const QString candidate = QDir(base).filePath(path);
        if (QFileInfo::exists(candidate))
            return QFileInfo(candidate).absoluteFilePath();
    }
    return path;
}

// 从文件路径加载 QPixmap
QPixmap InspectionWindow::loadPixmap(const QString &path) const
{
    QPixmap px;
    if (!path.isEmpty())
        px.load(path);
    return px;
}

// 缩放并显示标注图（文件模式）；IPC 模式由 ShmImageLabel 浅拷绘制
void InspectionWindow::updateImageView()
{
    if (m_ipcDisplayActive)
        return;

    const QPixmap &src = !m_annotatedPixmap.isNull() ? m_annotatedPixmap : m_loadedPixmap;
    if (src.isNull())
    {
        m_imageLabel->clear();
        m_imageLabel->setText(QStringLiteral("等待检测图像…"));
        m_imageLabel->setStyleSheet(QStringLiteral("background: transparent; color: #aaa;"));
        return;
    }

    const QSize target = m_imageLabel->size();
    if (target.width() < 10 || target.height() < 10)
    {
        m_imageLabel->setPixmap(src);
        return;
    }
    m_imageLabel->setPixmap(src.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// 通过文件对话框手动加载本地图片
void InspectionWindow::onLoadImage()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("从文件加载图片"), QDir::currentPath(),
                                                      QStringLiteral("图像 (*.png *.jpg *.bmp);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    m_loadedPixmap = loadPixmap(path);
    m_annotatedPixmap = QPixmap();
    m_annotatedPath.clear();
    updateImageView();
}

// 将当前显示的图像保存到文件
void InspectionWindow::onSaveImage()
{
    const QPixmap px = m_imageLabel->pixmap();
    if (px.isNull())
    {
        QMessageBox::warning(this, QStringLiteral("另存为图片"), QStringLiteral("当前没有可保存的图像。"));
        return;
    }

    const QString base = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString suggested = QStringLiteral("inspection_%1.png").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("另存为图片"), QDir(base).filePath(suggested),
                                                    QStringLiteral("PNG (*.png);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;
    if (!px.save(path))
        QMessageBox::warning(this, QStringLiteral("另存为图片"), QStringLiteral("写入失败：\n%1").arg(path));
}

// 导出 OK/NG 统计 JSON 及面板截图
void InspectionWindow::onSavePieData()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(base);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss"));
    const QString suggested = QStringLiteral("ok_ng_stats_%1.json").arg(stamp);
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("另存为统计数据"), QDir(base).filePath(suggested),
                                                QStringLiteral("JSON (*.json);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    QFileInfo fi(path);
    fi.dir().mkpath(QStringLiteral("."));
    if (!m_statsPanel->saveToFile(path))
    {
        QMessageBox::warning(this, QStringLiteral("另存为统计数据"), QStringLiteral("写入失败：\n%1").arg(path));
        return;
    }

    const QString pngPath = fi.path() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".png");
    QPixmap shot = m_statsPanel->grab();
    if (!shot.isNull())
        shot.save(pngPath);

    QMessageBox::information(this, QStringLiteral("另存为统计数据"),
                             QStringLiteral("已保存：\n%1\n%2").arg(path, pngPath));
}

// 在当前目录或上级目录查找 vision_engine.json
QString InspectionWindow::resolveWorkDir() const
{
    QString workDir = QDir::currentPath();
    if (!QFileInfo::exists(QDir(workDir).filePath(QStringLiteral("vision_engine.json"))))
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QString parent = QFileInfo(appDir).dir().absolutePath();
        if (QFileInfo::exists(QDir(parent).filePath(QStringLiteral("vision_engine.json"))))
            workDir = parent;
    }
    return workDir;
}
