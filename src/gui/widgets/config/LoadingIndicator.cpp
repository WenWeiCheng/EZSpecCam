#include "LoadingIndicator.h"

#include <QPainter>
#include <QHideEvent>

LoadingIndicator::LoadingIndicator(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(24, 24);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &LoadingIndicator::updateAnimation);
}

void LoadingIndicator::startAnimation()
{
    if (!m_timer->isActive()) {
        m_timer->start(m_timerInterval);
    }
}

void LoadingIndicator::stopAnimation()
{
    m_timer->stop();
}

bool LoadingIndicator::isAnimated() const
{
    return m_timer->isActive();
}

void LoadingIndicator::setColor(const QColor &color)
{
    if (m_color != color) {
        m_color = color;
        update();
    }
}

QColor LoadingIndicator::color() const
{
    return m_color;
}

void LoadingIndicator::setLineWidth(int width)
{
    if (m_lineWidth != width && width > 0) {
        m_lineWidth = width;
        update();
    }
}

int LoadingIndicator::lineWidth() const
{
    return m_lineWidth;
}

void LoadingIndicator::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect().adjusted(m_lineWidth, m_lineWidth, -m_lineWidth, -m_lineWidth);

    QPen pen(m_color);
    pen.setWidth(m_lineWidth);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    painter.drawArc(rect, m_angle * 16, 270 * 16);
}

void LoadingIndicator::hideEvent(QHideEvent *event)
{
    m_timer->stop();
    QWidget::hideEvent(event);
}

void LoadingIndicator::updateAnimation()
{
    m_angle = (m_angle + 30) % 360;
    update();
}