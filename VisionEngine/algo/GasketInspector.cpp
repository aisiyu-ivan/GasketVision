#include "GasketInspector.h"

#include <QJsonArray>
#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

// 从 JSON 加载像素比例、规格容差、图像中心及模板路径
bool GasketInspector::configure(const QJsonObject &config)
{
    m_pxPerMm = config.value(QStringLiteral("pxPerMm")).toDouble(10.0);
    m_matchScoreMin = config.value(QStringLiteral("matchScoreMin")).toDouble(0.45);
    m_templatePath = config.value(QStringLiteral("templatePath")).toString();

    const QJsonArray center = config.value(QStringLiteral("imageCenterPx")).toArray();
    if (center.size() >= 2)
    {
        m_imageCenter.x = static_cast<float>(center.at(0).toDouble(320.0));
        m_imageCenter.y = static_cast<float>(center.at(1).toDouble(240.0));
    }

    const QJsonObject spec = config.value(QStringLiteral("spec")).toObject();
    m_spec.outerDiameterMm = spec.value(QStringLiteral("outerDiameterMm")).toDouble(12.0);
    m_spec.innerDiameterMm = spec.value(QStringLiteral("innerDiameterMm")).toDouble(8.0);
    m_spec.outerTolMm = spec.value(QStringLiteral("outerTolMm")).toDouble(0.08);
    m_spec.innerTolMm = spec.value(QStringLiteral("innerTolMm")).toDouble(0.08);
    m_spec.offsetTolMm = spec.value(QStringLiteral("offsetTolMm")).toDouble(0.05);

    return loadTemplate();
}

// 从磁盘加载灰度定位模板
bool GasketInspector::loadTemplate()
{
    if (m_templatePath.isEmpty())
        return false;
    m_template = cv::imread(m_templatePath.toStdString(), cv::IMREAD_GRAYSCALE);
    return !m_template.empty();
}

// 在 ROI 内二值化后沿多方向径向扫描，中值统计内外径
bool GasketInspector::measureRing(const cv::Mat &gray, cv::Point2f &outCenter, double &outOdMm, double &outIdMm) const
{
    const double expectedOdPx = m_spec.outerDiameterMm * m_pxPerMm;
    const double expectedIdPx = m_spec.innerDiameterMm * m_pxPerMm;
    const int roiHalf = static_cast<int>(expectedOdPx * 0.5 + 24);

    cv::Rect roi(static_cast<int>(m_imageCenter.x) - roiHalf,
                 static_cast<int>(m_imageCenter.y) - roiHalf,
                 roiHalf * 2,
                 roiHalf * 2);
    roi &= cv::Rect(0, 0, gray.cols, gray.rows);
    if (roi.width < 40 || roi.height < 40)
        return false;

    cv::Mat patch;
    gray(roi).copyTo(patch);
    cv::GaussianBlur(patch, patch, cv::Size(5, 5), 1.0);

    cv::Mat bright;
    cv::threshold(patch, bright, 120, 255, cv::THRESH_BINARY);

    const int brightPixels = cv::countNonZero(bright);
    if (brightPixels < static_cast<int>(expectedOdPx * expectedOdPx * 0.04))
        return false;

    cv::Moments m = cv::moments(bright, true);
    if (m.m00 < 1.0)
        return false;

    const cv::Point2f originInRoi(m_imageCenter.x - static_cast<float>(roi.x),
                                    m_imageCenter.y - static_cast<float>(roi.y));
    outCenter = cv::Point2f(static_cast<float>(m.m10 / m.m00), static_cast<float>(m.m01 / m.m00))
                + cv::Point2f(static_cast<float>(roi.x), static_cast<float>(roi.y));

    const double minInnerR = expectedIdPx * 0.35;
    const double maxOuterR = expectedOdPx * 0.65;
    std::vector<double> innerRadii;
    std::vector<double> outerRadii;
    innerRadii.reserve(180);
    outerRadii.reserve(180);

    for (int deg = 0; deg < 360; deg += 2)
    {
        const double rad = deg * CV_PI / 180.0;
        const float dx = static_cast<float>(std::cos(rad));
        const float dy = static_cast<float>(std::sin(rad));

        int firstBright = -1;
        int lastBright = -1;
        const int maxStep = static_cast<int>(maxOuterR + 8);
        for (int step = 1; step <= maxStep; ++step)
        {
            const int x = static_cast<int>(originInRoi.x + dx * step);
            const int y = static_cast<int>(originInRoi.y + dy * step);
            if (x < 0 || y < 0 || x >= bright.cols || y >= bright.rows)
                break;

            const bool isBright = bright.at<uchar>(y, x) > 0;
            if (isBright && firstBright < 0)
                firstBright = step;
            if (isBright)
                lastBright = step;
        }

        if (firstBright < 0 || lastBright <= firstBright)
            continue;

        const double innerR = firstBright;
        const double outerR = lastBright;
        if (innerR >= minInnerR && outerR <= maxOuterR && outerR > innerR + 2.0)
        {
            innerRadii.push_back(innerR);
            outerRadii.push_back(outerR);
        }
    }

    if (innerRadii.size() < 24 || outerRadii.size() < 24)
        return false;

    auto medianOf = [](std::vector<double> values) -> double {
        const size_t mid = values.size() / 2;
        std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(mid), values.end());
        return values[mid];
    };

    const double innerRadiusPx = medianOf(std::move(innerRadii));
    const double outerRadiusPx = medianOf(std::move(outerRadii));
    outOdMm = std::round(outerRadiusPx * 2.0 / m_pxPerMm * 100.0) / 100.0;
    outIdMm = std::round(innerRadiusPx * 2.0 / m_pxPerMm * 100.0) / 100.0;
    return true;
}

// 绘制测量圆、基准中心点及 OK/NG 文字标注
void GasketInspector::drawAnnotation(GasketInspectResult &result, const cv::Mat &gray, cv::Point2f center, double odPx, double idPx) const
{
    cv::Mat canvas;
    cv::cvtColor(gray, canvas, cv::COLOR_GRAY2BGR);
    if (odPx > 1.0)
        cv::circle(canvas, center, static_cast<int>(odPx * 0.5), cv::Scalar(0, 220, 0), 2);
    if (idPx > 1.0)
        cv::circle(canvas, center, static_cast<int>(idPx * 0.5), cv::Scalar(0, 180, 255), 2);
    cv::circle(canvas, m_imageCenter, 3, cv::Scalar(255, 0, 0), -1);

    const QString label = result.ok ? QStringLiteral("OK") : QStringLiteral("NG");
    const cv::Scalar color = result.ok ? cv::Scalar(0, 220, 0) : cv::Scalar(0, 0, 255);
    cv::putText(canvas, label.toStdString(), cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX, 1.2, color, 2);
    result.annotated = canvas;
}

// 执行模板匹配、圆环测径及容差判定，输出完整检测结果
GasketInspectResult GasketInspector::inspect(const cv::Mat &image, const QString &sourcePath, int stationId)
{
    Q_UNUSED(stationId);
    GasketInspectResult result;
    result.imagePath = sourcePath;
    result.ok = false;
    result.defect = QStringLiteral("缺件");

    if (image.empty())
        return result;

    cv::Mat gray;
    if (image.channels() == 1)
        gray = image;
    else
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    double matchScore = 0.0;
    if (!m_template.empty() && gray.cols >= m_template.cols && gray.rows >= m_template.rows)
    {
        cv::Mat matchResult;
        cv::matchTemplate(gray, m_template, matchResult, cv::TM_CCOEFF_NORMED);
        double minVal = 0.0;
        double maxVal = 0.0;
        cv::minMaxLoc(matchResult, &minVal, &maxVal);
        matchScore = maxVal;
    }
    result.matchScore = matchScore;

    if (matchScore < m_matchScoreMin)
    {
        result.defect = QStringLiteral("缺件");
        drawAnnotation(result, gray, m_imageCenter, 0.0, 0.0);
        return result;
    }

    cv::Point2f partCenter = m_imageCenter;
    if (!measureRing(gray, partCenter, result.outerDiameterMm, result.innerDiameterMm))
    {
        result.defect = QStringLiteral("缺件");
        drawAnnotation(result, gray, m_imageCenter, 0.0, 0.0);
        const QString annotatedPath = sourcePath + QStringLiteral(".annotated.png");
        result.annotatedImagePath = annotatedPath;
        if (!result.annotated.empty())
            cv::imwrite(annotatedPath.toStdString(), result.annotated);
        return result;
    }

    result.offsetXMm = (partCenter.x - m_imageCenter.x) / m_pxPerMm;
    result.offsetYMm = (partCenter.y - m_imageCenter.y) / m_pxPerMm;

    const double odErr = std::abs(result.outerDiameterMm - m_spec.outerDiameterMm);
    const double idErr = std::abs(result.innerDiameterMm - m_spec.innerDiameterMm);
    const double offErr = std::hypot(result.offsetXMm, result.offsetYMm);

    bool ok = true;
    QString defect;
    if (odErr > m_spec.outerTolMm)
    {
        ok = false;
        defect = QStringLiteral("尺寸超差");
    }
    else if (idErr > m_spec.innerTolMm)
    {
        ok = false;
        defect = QStringLiteral("尺寸超差");
    }
    else if (offErr > m_spec.offsetTolMm)
    {
        ok = false;
        defect = QStringLiteral("偏心偏移");
    }

    result.ok = ok;
    result.defect = ok ? QString() : defect;
    drawAnnotation(result, gray, partCenter, result.outerDiameterMm * m_pxPerMm, result.innerDiameterMm * m_pxPerMm);

    const QString annotatedPath = sourcePath + QStringLiteral(".annotated.png");
    result.annotatedImagePath = annotatedPath;
    if (!result.annotated.empty())
        cv::imwrite(annotatedPath.toStdString(), result.annotated);

    return result;
}
