#ifndef HISTOGRAMVIEWWIDGET_H
#define HISTOGRAMVIEWWIDGET_H

#include <QWidget>
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

    void setXAxisLabel(const QString &label);
    void setYAxisLabel(const QString &label);

protected:
    void showEvent(QShowEvent *event) override;

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
};

#endif // HISTOGRAMVIEWWIDGET_H
