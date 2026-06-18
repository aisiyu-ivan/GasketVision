#include "ShmImageLabel.h"

#include <QPainter>
#include <QPaintEvent>

ShmImageLabel::ShmImageLabel(QWidget *parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ShmImageLabel::setShmImageView(const QImage &image, quint64 frameId)
{
    m_view = image;
    m_pendingFrameId = frameId;
    update();
}

void ShmImageLabel::clearShmView()
{
    m_view = QImage();
    m_pendingFrameId = 0;
    clear();
}

void ShmImageLabel::paintEvent(QPaintEvent *event)
{
    QLabel::paintEvent(event);

    if (m_view.isNull())
        return;

    QPainter painter(this);
    const QRect target = contentsRect();
    if (target.width() < 2 || target.height() < 2)
        return;

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(target, m_view);

    if (m_pendingFrameId != 0 && m_pendingFrameId != m_lastPaintedFrameId)
    {
        m_lastPaintedFrameId = m_pendingFrameId;
        emit framePainted(m_pendingFrameId);
    }
}
