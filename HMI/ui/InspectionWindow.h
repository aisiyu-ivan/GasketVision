#ifndef INSPECTIONWINDOW_H
#define INSPECTIONWINDOW_H

#include "InspectionResult.h"
#include "OkNgStatsPanel.h"
#include "ShmImageLabel.h"

#include <QHash>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QMainWindow>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>

class InspectionDataService;

// 视觉质检 HMI 主窗口：左侧图像 + 右侧 OK/NG 饼图统计；套接字就绪后分离启动 Engine/Camera（非父子进程）
class InspectionWindow : public QMainWindow
{
    Q_OBJECT

public:
    // 构建界面布局、初始化 OK/NG 面板并启动数据服务
    explicit InspectionWindow(QWidget *parent = nullptr);
    // 析构主窗口
    ~InspectionWindow() override;

    // 根据检测结果更新图像与 OK/NG 状态标签
    void applyInspectionResult(const InspectionResult &result);
    // 更新右侧 OK/NG 饼图计数
    void setPieCounts(const QHash<QString, unsigned int> &counts);

signals:
    // IPC 帧落盘并 paint 完成后通知释放相机存储区槽
    void ipcDisplayFinished(quint64 frameId);

protected:
    // 窗口尺寸变化时重绘图像
    void resizeEvent(QResizeEvent *event) override;
    // 首次显示时刷新模式标签样式
    void showEvent(QShowEvent *event) override;

private slots:
    // 通过文件对话框手动加载本地图片
    void onLoadImage();
    // 将当前显示的图像保存到文件
    void onSaveImage();
    // 导出 OK/NG 统计 JSON 及面板截图
    void onSavePieData();
    void onIpcFramePainted(quint64 frameId);
    void onIpcPaintFallback();

private:
    bool saveIpcImagesToDisk(const InspectionResult &result, QString *errorOut);
    // 加载 OK/NG 计数配置
    void initStatsPanel();
    // 按编译 profile 固定工作目录与窗口标题
    void initProfile();
    // 缩放并显示当前选中的原图或标注图
    void updateImageView();
    // 本地套接字就绪后分离启动 VisionEngine / CameraService
    void onControlListenReady();
    // 结束可能残留的 VisionEngine / CameraService（正式与测试共用 IPC，切换前须清理）
    static void stopBackendProcesses();
    // 用 startDetached 按顺序启动 Engine → Camera（仅执行一次）
    bool launchDetachedPipeline();
    // 校验测试目录下样品图与定位模板
    bool validateTestAssets(const QString &workDir, QString *errorOut) const;
    // 刷新顶部「相机模式 / 测试模式」标签颜色与文字
    void updateModeLabelStyle();
    // 更新状态栏灰色提示文字
    void setStatusMessage(const QString &text);
    // 当前 profile 对应的模式显示名
    QString modeDisplayName() const;
    // 解析正式环境工作目录（含 vision_engine.json）
    QString resolveCameraWorkDir() const;
    // 解析测试环境工作目录（test/vision_engine.json）
    QString resolveTestWorkDir() const;
    // 在当前目录或上级目录查找 vision_engine.json
    QString resolveWorkDir() const;
    // 在多个基准目录中解析图像相对路径
    QString resolveImagePath(const QString &path) const;
    // 从文件路径加载 QPixmap
    QPixmap loadPixmap(const QString &path) const;
    QPushButton *m_btnLoadImage = nullptr;     // 「从文件加载图片」按钮
    QPushButton *m_btnSaveImage = nullptr;     // 「另存为图片」按钮
    QPushButton *m_btnSavePieData = nullptr;   // 「另存为统计数据」按钮
    QLabel *m_resultLabel = nullptr;           // 顶部「状态:」标签
    QLabel *m_modeLabel = nullptr;             // 顶部模式标签（相机/测试）
    QLabel *m_statusLabel = nullptr;           // 顶部状态/OK-NG 文字
    QWidget *m_headerBar = nullptr;            // 顶部工具栏容器
    QLabel *m_imageLabel = nullptr;            // 文件模式图像显示区
    ShmImageLabel *m_shmImageLabel = nullptr;  // IPC 浅拷 SHM 视图显示
    OkNgStatsPanel *m_statsPanel = nullptr;    // 右侧 OK/NG 统计面板
    InspectionDataService *m_dataService = nullptr; // 检测数据服务
    bool m_pipelineReady = false;              // 本地套接字是否已就绪
    bool m_pipelineLaunchAttempted = false;      // 是否已尝试分离启动 Engine/Camera
    QString m_activeWorkDir;                     // 当前 profile 工作目录

    QString m_annotatedPath;                     // 标注图文件路径
    QPixmap m_loadedPixmap;                      // 手动加载图像（文件模式）
    QPixmap m_annotatedPixmap;                 // 标注图像素缓存（文件模式）
    bool m_ipcDisplayActive = false;           // 当前是否 IPC 浅拷显示
    QImage m_ipcAnnotatedView;                 // 相机 SHM 标注浅拷视图
    quint64 m_pendingIpcReleaseFrameId = 0;
    QTimer *m_ipcPaintFallbackTimer = nullptr;
    QString m_lastOkNg = QStringLiteral("—");  // 最近一次 OK/NG 文字
};

#endif // INSPECTIONWINDOW_H
