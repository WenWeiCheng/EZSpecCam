#include "SpectrumViewWidget.h"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QDebug>
#include <algorithm>
#include <limits>
#include "../../qcustomplot.h"

SpectrumViewWidget::SpectrumViewWidget(QWidget *parent)
    : QWidget(parent)
    , m_plot(nullptr)
    , m_graph(nullptr)
    , m_cursorLine(nullptr)
    , m_cursorLabel(nullptr)
    , m_dataValid(false)
    , m_xAxisLabel("X (pixels)")
    , m_yAxisLabel("Intensity")
    , m_rubberBand(nullptr)
{
    m_plot = new QCustomPlot(this);
    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_plot);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setupPlot();
}

SpectrumViewWidget::~SpectrumViewWidget()
{
}

void SpectrumViewWidget::setupPlot()
{
    m_plot->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_graph = m_plot->addGraph(m_plot->xAxis, m_plot->yAxis);
    m_graph->setLineStyle(QCPGraph::lsLine);
    m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));
    m_graph->setPen(QPen(Qt::blue, 1.0));

    m_graph->setAdaptiveSampling(true);

    m_plot->xAxis->setLabel(m_xAxisLabel);
    m_plot->yAxis->setLabel(m_yAxisLabel);
    m_plot->xAxis->setRange(0, 100);
    m_plot->yAxis->setRange(0, 100);

    m_plot->setInteractions(QCP::iSelectPlottables);
    m_plot->setMouseTracking(true);
    m_plot->installEventFilter(this);
    m_plot->axisRect()->setAutoMargins(QCP::msNone);
    m_plot->axisRect()->setMargins(QMargins(80, 20, 20, 50));

    m_plot->setNoAntialiasingOnDrag(true);

    m_cursorLine = new QCPItemLine(m_plot);
    m_cursorLine->setPen(QPen(Qt::red, 1, Qt::DashLine));
    m_cursorLine->setVisible(false);
    m_cursorLine->start->setCoords(0, 0);
    m_cursorLine->end->setCoords(0, 1);

    m_cursorLabel = new QCPItemText(m_plot);
    m_cursorLabel->setPen(QPen(Qt::black));
    m_cursorLabel->setBrush(QBrush(Qt::white));
    m_cursorLabel->setFont(QFont("sans", 9));
    m_cursorLabel->setText("");
    m_cursorLabel->setVisible(false);
    m_cursorLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_cursorLabel->position->setCoords(0, 0);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumViewWidget::setData(const QVector<double> &x, const QVector<double> &y)
{
    if (x.size() != y.size()) {
        return;
    }

    if (x.isEmpty()) {
        return;
    }

    m_xData = x;
    m_yData = y;
    m_dataValid = true;

    m_graph->setData(x, y);

    if (!m_userHasZoomed) {
        applyXAxisRange();
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumViewWidget::setFromImage(const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    if (image.height() != 1) {
        return;
    }

    QVector<double> xData;
    QVector<double> yData = extractRowData(image);

    int width = image.width();
    xData.reserve(width);

    for (int i = 0; i < width; ++i) {
        xData.append(i);
    }

    setData(xData, yData);
}

void SpectrumViewWidget::setSpectrumData(const QVector<quint64> &spectrum)
{
    if (spectrum.isEmpty()) {
        return;
    }

    QVector<double> xData;
    QVector<double> yData;
    xData.reserve(spectrum.size());
    yData.reserve(spectrum.size());

    for (int i = 0; i < spectrum.size(); ++i) {
        xData.append(i);
        yData.append(static_cast<double>(spectrum[i]));
    }

    setData(xData, yData);
}

QVector<double> SpectrumViewWidget::xData() const
{
    return m_xData;
}

QVector<double> SpectrumViewWidget::yData() const
{
    return m_yData;
}

bool SpectrumViewWidget::hasData() const
{
    return m_dataValid;
}

void SpectrumViewWidget::clearData()
{
    m_xData.clear();
    m_yData.clear();
    m_dataValid = false;

    m_graph->data()->clear();
    m_plot->xAxis->setRange(0, 100);
    m_plot->yAxis->setRange(0, 100);

    m_cursorLine->setVisible(false);
    m_cursorLabel->setVisible(false);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

double SpectrumViewWidget::intensityAt(double x) const
{
    if (!m_dataValid || m_xData.isEmpty()) {
        return 0.0;
    }

    if (x < m_xData.first() || x > m_xData.last()) {
        return 0.0;
    }

    for (int i = 0; i < m_xData.size() - 1; ++i) {
        if (x >= m_xData[i] && x <= m_xData[i + 1]) {
            if (m_xData[i + 1] == m_xData[i]) {
                return m_yData[i];
            }
            double t = (x - m_xData[i]) / (m_xData[i + 1] - m_xData[i]);
            return m_yData[i] + t * (m_yData[i + 1] - m_yData[i]);
        }
    }

    return 0.0;
}

void SpectrumViewWidget::setXAxisLabel(const QString &label)
{
    m_xAxisLabel = label;
    m_plot->xAxis->setLabel(label);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumViewWidget::setYAxisLabel(const QString &label)
{
    m_yAxisLabel = label;
    m_plot->yAxis->setLabel(label);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumViewWidget::setXAxisRangeMode(XAxisRangeMode mode)
{
    if (m_xAxisRangeMode == mode) {
        return;
    }

    m_xAxisRangeMode = mode;
    m_userHasZoomed = false;

    if (m_dataValid && !m_xData.isEmpty()) {
        applyXAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SpectrumViewWidget::setCustomXRange(double min, double max)
{
    m_customXMin = min;
    m_customXMax = max;

    if (m_xAxisRangeMode == XAxisRangeMode::Custom && m_dataValid) {
        applyXAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SpectrumViewWidget::setIntensityScaleType(IntensityScaleType type)
{
    if (m_intensityScaleType == type) {
        return;
    }

    m_intensityScaleType = type;

    switch (m_intensityScaleType) {
        case IntensityScaleType::Auto: {
            m_plot->yAxis->setScaleType(QCPAxis::stLinear);
            auto ticker = QSharedPointer<QCPAxisTicker>(new QCPAxisTicker());
            m_plot->yAxis->setTicker(ticker);
            break;
        }
        case IntensityScaleType::Log: {
            m_plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
            auto ticker = QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog());
            m_plot->yAxis->setTicker(ticker);
            break;
        }
    }

    if (m_dataValid && !m_xData.isEmpty()) {
        applyXAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SpectrumViewWidget::applyXAxisRange()
{
    if (!m_dataValid || m_xData.isEmpty()) {
        return;
    }

    double minX = m_xData.first();
    double maxX = m_xData.last();
    double range = maxX - minX;

    switch (m_xAxisRangeMode) {
        case XAxisRangeMode::Auto: {
            double minY = *std::min_element(m_yData.constBegin(), m_yData.constEnd());
            double maxY = *std::max_element(m_yData.constBegin(), m_yData.constEnd());
            double yPadding = (maxY - minY) * 0.02;
            if (yPadding < 1.0) yPadding = 1.0;
            m_plot->xAxis->setRange(minX - range * 0.02, maxX + range * 0.02);
            m_plot->yAxis->setRange(minY - yPadding, maxY + yPadding);
            break;
        }
        case XAxisRangeMode::Full:
            m_plot->xAxis->setRange(minX, maxX);
            break;
        case XAxisRangeMode::ZoomLeft:
            m_plot->xAxis->setRange(minX, minX + range * 0.5);
            break;
        case XAxisRangeMode::ZoomRight:
            m_plot->xAxis->setRange(maxX - range * 0.5, maxX);
            break;
        case XAxisRangeMode::ZoomCenter:
            m_plot->xAxis->setRange(minX + range * 0.25, maxX - range * 0.25);
            break;
        case XAxisRangeMode::Custom:
            m_plot->xAxis->setRange(m_customXMin, m_customXMax);
            break;
    }
}

void SpectrumViewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dataValid || m_xData.isEmpty()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    double dataX = widgetToDataX(event->pos().x());
    updateCursor(dataX);

    double intensity = intensityAt(dataX);
    emit cursorPosition(dataX, intensity);

    QWidget::mouseMoveEvent(event);
}

void SpectrumViewWidget::leaveEvent(QEvent *event)
{
    m_cursorLine->setVisible(false);
    m_cursorLabel->setVisible(false);
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    emit cursorLeft();

    QWidget::leaveEvent(event);
}

void SpectrumViewWidget::resizeEvent(QResizeEvent *event)
{
    if (m_plot) {
        m_plot->resize(size());
    }
    QWidget::resizeEvent(event);
}

void SpectrumViewWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    if (m_plot && m_dataValid) {
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

bool SpectrumViewWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_plot) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                QRect axisRect = m_plot->axisRect()->rect();
                if (axisRect.contains(me->pos())) {
                    m_rubberBandOrigin = me->pos();
                    m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
                    m_rubberBand->show();
                    return true;
                }
            } else if (me->button() == Qt::RightButton) {
                resetZoomToFit();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_rubberBand->isVisible()) {
                m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, me->pos()).normalized());
                return true;
            }
            mouseMoveEvent(me);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_rubberBand->isVisible()) {
                m_rubberBand->hide();
                if (me->button() == Qt::LeftButton) {
                    QRectF selectionRect = QRectF(m_rubberBandOrigin, me->pos()).normalized();
                    double x1 = m_plot->xAxis->pixelToCoord(selectionRect.left());
                    double x2 = m_plot->xAxis->pixelToCoord(selectionRect.right());
                    double y1 = m_plot->yAxis->pixelToCoord(selectionRect.top());
                    double y2 = m_plot->yAxis->pixelToCoord(selectionRect.bottom());
                    if (qAbs(x2 - x1) > 0 && qAbs(y2 - y1) > 0) {
                        m_plot->xAxis->setRange(x1, x2);
                        m_plot->yAxis->setRange(y1, y2);
                        m_plot->replot(QCustomPlot::rpQueuedReplot);
                        m_userHasZoomed = true;
                    }
                }
                return true;
            }
        } else if (event->type() == QEvent::Leave) {
            leaveEvent(event);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SpectrumViewWidget::updateCursor(double x)
{
    if (!m_dataValid) {
        return;
    }

    double minY = m_plot->yAxis->range().lower;
    double maxY = m_plot->yAxis->range().upper;

    m_cursorLine->start->setCoords(x, minY);
    m_cursorLine->end->setCoords(x, maxY);
    m_cursorLine->setVisible(true);

    QString labelText = QString("X: %1\nI: %2")
        .arg(x, 0, 'f', 1)
        .arg(intensityAt(x), 0, 'f', 0);

    m_cursorLabel->setText(labelText);

    double labelX = m_plot->xAxis->range().lower + 5;

    m_cursorLabel->position->setCoords(labelX, maxY - (maxY - minY) * 0.05);
    m_cursorLabel->setVisible(true);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

double SpectrumViewWidget::widgetToDataX(int widgetX) const
{
    if (!m_dataValid || m_xData.isEmpty()) {
        return 0.0;
    }

    QRect axisRect = m_plot->axisRect()->rect();

    if (widgetX < axisRect.left() || widgetX > axisRect.right()) {
        return m_xData.first();
    }

    double rangeSpan = m_plot->xAxis->range().upper - m_plot->xAxis->range().lower;
    double axisWidth = axisRect.width();

    double ratio = static_cast<double>(widgetX - axisRect.left()) / axisWidth;
    double dataX = m_plot->xAxis->range().lower + ratio * rangeSpan;

    return dataX;
}

QVector<double> SpectrumViewWidget::extractRowData(const QImage &image) const
{
    QVector<double> data;

    if (image.isNull()) {
        return data;
    }

    int width = image.width();
    data.reserve(width);

    if (image.format() == QImage::Format_Grayscale16) {
        const ushort *gray16 = reinterpret_cast<const ushort *>(image.constBits());
        for (int x = 0; x < width; ++x) {
            data.append(static_cast<double>(gray16[x]));
        }
    } else if (image.format() == QImage::Format_Grayscale8) {
        const uchar *gray8 = image.constBits();
        for (int x = 0; x < width; ++x) {
            data.append(static_cast<double>(gray8[x]));
        }
    } else {
        for (int x = 0; x < width; ++x) {
            QRgb pixel = image.pixel(x, 0);
            data.append(static_cast<double>(qGray(pixel)));
        }
    }

    return data;
}

void SpectrumViewWidget::resetZoomToFit()
{
    if (!m_dataValid || m_xData.isEmpty()) {
        return;
    }

    m_userHasZoomed = false;
    applyXAxisRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}