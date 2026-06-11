#include "SyntheticImageSource.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMap>

#include <opencv2/imgcodecs.hpp>

namespace
{
// 从文件名提取缺陷类别前缀（如 ok_001 → ok）
QString caseKeyFromBaseName(const QString &fileName)
{
    const QString stem = QFileInfo(fileName).completeBaseName();
    const int sep = stem.lastIndexOf(QLatin1Char('_'));
    if (sep <= 0)
        return stem;
    const QString tail = stem.mid(sep + 1);
    for (const QChar c : tail)
    {
        if (!c.isDigit())
            return stem;
    }
    return stem.left(sep);
}

// 按 ok → 尺寸/偏心/缺件 顺序交错排列，避免连续播放同类样本
QStringList interleaveByCase(const QStringList &paths)
{
    static const QStringList kOrder = {
        QStringLiteral("ok"),
        QStringLiteral("od_oversize"),
        QStringLiteral("id_undersize"),
        QStringLiteral("eccentric"),
        QStringLiteral("missing"),
    };

    QMap<QString, QStringList> groups;
    for (const QString &path : paths)
        groups[caseKeyFromBaseName(path)].append(path);

    for (auto it = groups.begin(); it != groups.end(); ++it)
        it.value().sort();

    int maxCount = 0;
    for (const QStringList &list : groups)
        maxCount = qMax(maxCount, list.size());

    QStringList out;
    out.reserve(paths.size());
    for (int i = 0; i < maxCount; ++i)
    {
        for (const QString &key : kOrder)
        {
            const QStringList &list = groups.value(key);
            if (i < list.size())
                out.append(list.at(i));
        }
        for (auto it = groups.cbegin(); it != groups.cend(); ++it)
        {
            if (kOrder.contains(it.key()))
                continue;
            if (i < it.value().size())
                out.append(it.value().at(i));
        }
    }
    return out;
}
} // namespace

// 扫描样本目录，过滤标注图并按类别交错排序
bool SyntheticImageSource::open(const QJsonObject &config)
{
    close();
    const QJsonObject syn = config.value(QStringLiteral("synthetic")).toObject();
    QString folder = syn.value(QStringLiteral("folder")).toString(QStringLiteral("station1"));
    m_intervalMs = syn.value(QStringLiteral("intervalMs")).toInt(800);

    QDir dir(folder);
    if (!dir.exists())
        return false;

    const QStringList filters = {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.bmp")};
    m_files.clear();
    for (const QString &filter : filters)
    {
        const QStringList names = dir.entryList({filter}, QDir::Files, QDir::Name);
        for (const QString &name : names)
        {
            if (name.contains(QStringLiteral(".annotated."), Qt::CaseInsensitive))
                continue;
            m_files.append(dir.filePath(name));
        }
    }

    m_files = interleaveByCase(m_files);
    m_index = 0;
    m_open = !m_files.isEmpty();
    return m_open;
}

// 清空文件列表并重置播放索引
void SyntheticImageSource::close()
{
    m_files.clear();
    m_index = 0;
    m_open = false;
}

// 样本目录是否已成功打开
bool SyntheticImageSource::isConnected() const
{
    return m_open;
}

// 返回合成相机当前状态描述
QString SyntheticImageSource::statusText() const
{
    if (!m_open)
        return QStringLiteral("合成相机未就绪");
    return QStringLiteral("合成采集中");
}

// 循环读取下一张样本图并填充 VisionFrame
bool SyntheticImageSource::grabFrame(VisionFrame &out)
{
    if (!m_open || m_files.isEmpty())
        return false;

    const QString path = m_files.at(m_index);
    m_index = (m_index + 1) % m_files.size();

    cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_GRAYSCALE);
    if (img.empty())
        img = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
    if (img.empty())
        return false;

    out.image = img;
    out.sourcePath = QFileInfo(path).canonicalFilePath();
    if (out.sourcePath.isEmpty())
        out.sourcePath = path;
    out.timestampMs = QDateTime::currentMSecsSinceEpoch();
    return true;
}
