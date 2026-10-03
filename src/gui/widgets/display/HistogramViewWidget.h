#ifndef HISTOGRAMVIEWWIDGET_H
#define HISTOGRAMVIEWWIDGET_H

#include <QWidget>
#include <QPoint>
#include <QRubberBand>
#include <QString>
#include <QVector>

class QCPBars;
class QCustomPlot;

class HistogramViewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HistogramViewWidget(QWidget *parent = nullptr);
    ~HistogramViewWidget() override;

    //! bins 为各箱的中心值，counts 为对应像素数。两者长度须一致
    void setHistogram(const QVector<double> &bins, const QVector<double> &counts);
    void clearHistogram();
    bool hasHistogram() const { return m_hasData; }

    QVector<double> binCenters() const { return m_binCenters; }
    QVector<double> counts() const { return m_counts; }
    double binWidth() const { return m_binWidth; }

    bool isLogScale() const { return m_logScale; }
    void setLogScale(bool enabled);

    //! 用户是否框选缩放过。缩放过之后新帧不再重置横坐标量程
    bool isZoomed() const { return m_userHasZoomed; }
    void resetZoom();

    void setXAxisLabel(const QString &label);
    void setYAxisLabel(const QString &label);

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupPlot();
    void applyTheme();
    void applyAxisRange();
    void applyScaleType();

    QCustomPlot *m_plot;
    QCPBars *m_bars;
    QVector<double> m_binCenters;
    QVector<double> m_counts;
    double m_binWidth = 1.0;
    bool m_hasData = false;
    bool m_logScale = false;
    QString m_xAxisLabel;
    QString m_yAxisLabel;

    //! 框选缩放只动横坐标，纵坐标始终按数据自动
    QRubberBand *m_rubberBand = nullptr;
    QPoint m_rubberBandOrigin;
    bool m_userHasZoomed = false;
};

#endif // HISTOGRAMVIEWWIDGET_H
