#ifndef SHMIMAGELABEL_H
#define SHMIMAGELABEL_H

#include <QLabel>
#include <QImage>

// 直接绘制 SHM 浅拷 QImage 视图，避免 QPixmap::fromImage 整图深拷
class ShmImageLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ShmImageLabel(QWidget *parent = nullptr);

    void setShmImageView(const QImage &image, quint64 frameId);
    void clearShmView();

signals:
    void framePainted(quint64 frameId);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_view;
    quint64 m_pendingFrameId = 0;
    quint64 m_lastPaintedFrameId = 0;
};

#endif // SHMIMAGELABEL_H
