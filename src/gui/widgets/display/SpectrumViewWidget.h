#ifndef SPECTRUMVIEWWIDGET_H
#define SPECTRUMVIEWWIDGET_H

#include <QWidget>
#include <QImage>
#include <QVector>
#include <QMouseEvent>
#include <QShowEvent>
#include <QString>
#include <QRubberBand>

#include "../../qcustomplot.h"

class QCPGraph;
class QCPItemLine;
class QCPItemText;

class SpectrumViewWidget : public QWidget
{
    Q_OBJECT
public:
    enum class XAxisRangeMode {
        Auto,
        Full,
        ZoomLeft,
        ZoomRight,
        ZoomCenter,
        Custom
    };
    Q_ENUM(XAxisRangeMode)

    enum class LineStyle {
        Line,
        LineAndPoints,
        Points
    };
    Q_ENUM(LineStyle)

    enum class IntensityScaleType {
        Auto,
        Log
    };
    Q_ENUM(IntensityScaleType)

    explicit SpectrumViewWidget(QWidget *parent = nullptr);
    ~SpectrumViewWidget() override;

    void setData(const QVector<double> &x, const QVector<double> &y);
    void setFromImage(const QImage &image);
    void setSpectrumData(const QVector<quint64> &spectrum);

    QVector<double> xData() const;
    QVector<double> yData() const;
    bool hasData() const;
    void clearData();

    LineStyle lineStyle() const { return m_lineStyle; }
    void setLineStyle(LineStyle style);

    double intensityAt(double x) const;

    void setXAxisLabel(const QString &label);
    void setYAxisLabel(const QString &label);

    XAxisRangeMode xAxisRangeMode() const { return m_xAxisRangeMode; }
    void setXAxisRangeMode(XAxisRangeMode mode);
    void setCustomXRange(double min, double max);

    IntensityScaleType intensityScaleType() const { return m_intensityScaleType; }
    void setIntensityScaleType(IntensityScaleType type);

    double customXMin() const { return m_customXMin; }
    double customXMax() const { return m_customXMax; }
    int dataWidth() const { return m_xData.isEmpty() ? 0 : m_xData.size(); }

    double currentXMin() const { return m_plot->xAxis->range().lower; }
    double currentXMax() const { return m_plot->xAxis->range().upper; }

signals:
    void cursorPosition(double x, double intensity);
    void cursorLeft();

protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupPlot();
    void updateCursor(double x, double y);
    double widgetToDataX(int widgetX) const;
    QVector<double> extractRowData(const QImage &image) const;
    void applyXAxisRange();
    void resetZoomToFit();

    QCustomPlot *m_plot;
    QCPGraph *m_graph;
    QCPItemLine *m_cursorLine;
    QCPItemText *m_cursorLabel;
    QVector<double> m_xData;
    QVector<double> m_yData;
    bool m_dataValid;
    QString m_xAxisLabel;
    QString m_yAxisLabel;
    XAxisRangeMode m_xAxisRangeMode = XAxisRangeMode::Auto;
    LineStyle m_lineStyle = LineStyle::Line;
    IntensityScaleType m_intensityScaleType = IntensityScaleType::Auto;
    double m_customXMin = 0;
    double m_customXMax = 100;

    QRubberBand *m_rubberBand = nullptr;
    QPoint m_rubberBandOrigin;
    bool m_userHasZoomed = false;
};

#endif // SPECTRUMVIEWWIDGET_H