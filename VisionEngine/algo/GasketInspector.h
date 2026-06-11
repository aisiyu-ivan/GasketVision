#ifndef GASKETINSPECTOR_H
#define GASKETINSPECTOR_H

#include <QJsonObject>
#include <QString>

#include <opencv2/core.hpp>

// 垫片尺寸规格及各项容差（外径、内径、偏心）
struct GasketSpec
{
    double outerDiameterMm = 12.0; // 外径标称值（mm）
    double innerDiameterMm = 8.0; // 内径标称值（mm）
    double outerTolMm = 0.08; // 外径容差（mm）
    double innerTolMm = 0.08; // 内径容差（mm）
    double offsetTolMm = 0.05; // 偏心容差（mm）
};

// 单次垫片检测结果（尺寸、偏移、缺陷类型及标注图）
struct GasketInspectResult
{
    bool ok = false; // 是否 OK
    QString defect; // 缺陷中文描述
    QString imagePath; // 原图路径
    QString annotatedImagePath; // 标注图路径
    double outerDiameterMm = 0.0; // 实测外径（mm）
    double innerDiameterMm = 0.0; // 实测内径（mm）
    double offsetXMm = 0.0; // X 方向偏心（mm）
    double offsetYMm = 0.0; // Y 方向偏心（mm）
    double matchScore = 0.0; // 模板匹配得分
    cv::Mat annotated; // 标注图（内存）
};

// 垫片视觉检测器：模板匹配定位 + 圆环径向测径，判定 OK/NG
class GasketInspector
{
public:
    // 从 JSON 配置加载像素比例、规格容差及模板路径
    bool configure(const QJsonObject &config);

    // 对单帧图像执行完整检测流程并返回结果
    GasketInspectResult inspect(const cv::Mat &image, const QString &sourcePath, int stationId);

private:
    // 加载灰度定位模板图像
    bool loadTemplate();
    // 在 ROI 内通过径向扫描测量圆环内外径及中心
    bool measureRing(const cv::Mat &gray, cv::Point2f &outCenter, double &outOdMm, double &outIdMm) const;
    // 在图像上绘制测量圆、中心点及 OK/NG 标注
    void drawAnnotation(GasketInspectResult &result, const cv::Mat &gray, cv::Point2f center, double odPx, double idPx) const;

    GasketSpec m_spec; // 尺寸规格与容差
    double m_pxPerMm = 10.0; // 像素/mm 比例
    cv::Point2f m_imageCenter{320.f, 240.f}; // 图像基准中心（像素）
    double m_matchScoreMin = 0.45; // 模板匹配最低得分
    QString m_templatePath; // 定位模板路径
    cv::Mat m_template; // 灰度定位模板
};

#endif // GASKETINSPECTOR_H
