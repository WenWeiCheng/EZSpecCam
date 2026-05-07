#ifndef LOADINGINDICATOR_H
#define LOADINGINDICATOR_H

#include <QWidget>
#include <QColor>
#include <QTimer>

class LoadingIndicator : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)
    Q_PROPERTY(int lineWidth READ lineWidth WRITE setLineWidth)

public:
    explicit LoadingIndicator(QWidget *parent = nullptr);

    void startAnimation();
    void stopAnimation();
    bool isAnimated() const;

    void setColor(const QColor &color);
    QColor color() const;

    void setLineWidth(int width);
    int lineWidth() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void updateAnimation();

private:
    int m_angle = 0;
    QTimer *m_timer = nullptr;
    QColor m_color = Qt::darkGray;
    int m_lineWidth = 3;
    static constexpr int m_timerInterval = 25;
};

#endif // LOADINGINDICATOR_H