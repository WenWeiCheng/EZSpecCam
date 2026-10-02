#include "SpectrumViewWidget.h"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QDebug>
#include <algorithm>
#include <limits>
#include <cmath>
#include "../../Theme.h"
#include "../../qcustomplot.h"

namespace {
// 读数框字号。原来 9 磅在放大后的绘图区里显得偏小，看着像一坨看不清的小字
constexpr int kCursorFontPointSize = 12;
// 读数框距离绘图区左上角的像素
constexpr int kCursorLabelInset = 10;
} // namespace

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

    m_graph->setAdaptiveSampling(true);

    m_lineStyle = LineStyle::Line;
    m_graph->setLineStyle(QCPGraph::lsLine);
    m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));

    m_plot->xAxis->setLabel(m_xAxisLabel);
    m_plot->yAxis->setLabel(m_yAxisLabel);
    m_plot->xAxis->setRange(0, 100);
    m_plot->yAxis->setRange(0, 100);

    // 补出上、右两条轴，凑成四边封闭的图框。上、右只画轴线和刻度线，
    // 刻度标签仍留在左、下（setupFullAxesBox 已关掉），range 通过信号跟着左、下走
    m_plot->axisRect()->setupFullAxesBox(true);

    m_plot->setInteractions(QCP::iSelectPlottables);
    m_plot->setMouseTracking(true);
    m_plot->installEventFilter(this);
    m_plot->axisRect()->setAutoMargins(QCP::msNone);
    m_plot->axisRect()->setMargins(QMargins(80, 20, 20, 50));

    m_plot->setNoAntialiasingOnDrag(true);

    m_cursorLine = new QCPItemLine(m_plot);
    m_cursorLine->setVisible(false);
    m_cursorLine->start->setCoords(0, 0);
    m_cursorLine->end->setCoords(0, 1);

    m_cursorLabel = new QCPItemText(m_plot);
    m_cursorLabel->setFont(QFont("sans", kCursorFontPointSize));
    m_cursorLabel->setText("");
    m_cursorLabel->setVisible(false);
    m_cursorLabel->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_cursorLabel->position->setCoords(0, 0);

    m_cursorLine->setLayer(QLatin1String("overlay"));
    m_cursorLabel->setLayer(QLatin1String("overlay"));

    // 建完轴再上色：applyToPlot 要遍历 axisRect 里的四条轴
    applyTheme();
    connect(Theme::instance(), &Theme::themeChanged, this, &SpectrumViewWidget::applyTheme);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumViewWidget::applyTheme()
{
    const Theme::PlotColors &c = Theme::instance()->plotColors();

    Theme::instance()->applyToPlot(m_plot);

    m_graph->setPen(QPen(c.curve, 1.0));

    // 悬浮读数是一个不透明的色块，深色下不跟着换会变成一块白斑。
    // 注意 QCPItemText 的三处颜色是分开的：文字用 setColor()，setPen() 只管
    // 方框的边线，setBrush() 管方框的填充——只设 pen 的话字还是黑的
    m_cursorLabel->setColor(c.overlayText);
    m_cursorLabel->setPen(QPen(c.overlayText));
    m_cursorLabel->setBrush(QBrush(c.overlayBackground));

    QColor cursorColor = c.crosshair;
    cursorColor.setAlpha(255);
    m_cursorLine->setPen(QPen(cursorColor, 1, Qt::DashLine));

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
        applyAxisRange();
    }

    // 鼠标不动时读数会一直停在上一帧的数值上，live 模式下等于挂着一个假读数。
    // 只在光标确实还悬着的时候刷新，否则鼠标从没进过画面也会凭空冒出读数
    double readoutY = 0.0;
    const bool refreshReadout = m_cursorActive;
    if (refreshReadout) {
        readoutY = intensityAt(m_lastCursorX);
        applyCursor(m_lastCursorX, readoutY);
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);

    if (refreshReadout) {
        emit cursorPosition(m_lastCursorX, readoutY);
    }
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
    // 数据都清了，光标也就没有可读的值了，别让新数据进来时又冒出来
    m_cursorActive = false;

    m_graph->data()->clear();
    m_plot->xAxis->setRange(0, 100);
    m_plot->yAxis->setRange(0, 100);
    m_userHasZoomed = false;

    m_cursorLine->setVisible(false);
    m_cursorLabel->setVisible(false);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void SpectrumViewWidget::setLineStyle(LineStyle style)
{
    if (m_lineStyle == style) {
        return;
    }

    m_lineStyle = style;

    switch (style) {
        case LineStyle::Line:
            m_graph->setLineStyle(QCPGraph::lsLine);
            m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssNone));
            break;
        case LineStyle::LineAndPoints:
            m_graph->setLineStyle(QCPGraph::lsLine);
            m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4.0));
            break;
        case LineStyle::Points:
            m_graph->setLineStyle(QCPGraph::lsNone);
            m_graph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, 4.0));
            break;
    }

    if (m_dataValid && !m_xData.isEmpty()) {
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
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

void SpectrumViewWidget::setXAxisRangeMode(AxisRangeMode mode)
{
    if (m_xAxisRangeMode == mode) {
        return;
    }

    m_xAxisRangeMode = mode;
    m_userHasZoomed = false;

    if (m_dataValid && !m_xData.isEmpty()) {
        applyAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SpectrumViewWidget::setYAxisRangeMode(AxisRangeMode mode)
{
    if (m_yAxisRangeMode == mode) {
        return;
    }

    m_yAxisRangeMode = mode;
    m_userHasZoomed = false;

    if (m_dataValid && !m_xData.isEmpty()) {
        applyAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SpectrumViewWidget::setManualXRange(double min, double max)
{
    if (max <= min) {
        return;
    }

    m_manualXMin = min;
    m_manualXMax = max;

    if (m_xAxisRangeMode == AxisRangeMode::Manual && m_dataValid) {
        applyAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SpectrumViewWidget::setManualYRange(double min, double max)
{
    if (max <= min) {
        return;
    }

    m_manualYMin = min;
    m_manualYMax = max;

    if (m_yAxisRangeMode == AxisRangeMode::Manual && m_dataValid) {
        applyAxisRange();
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

    // 右侧轴共用左侧的刻度算法，否则对数刻度下它的刻度线会和左侧标签对不齐
    m_plot->yAxis2->setScaleType(m_plot->yAxis->scaleType());
    m_plot->yAxis2->setTicker(m_plot->yAxis->ticker());

    if (m_dataValid && !m_xData.isEmpty()) {
        applyAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SpectrumViewWidget::applyAxisRange()
{
    if (!m_dataValid || m_xData.isEmpty()) {
        return;
    }

    if (m_userHasZoomed) {
        return;
    }

    double minX = m_xData.first();
    double maxX = m_xData.last();
    double rangeX = maxX - minX;

    if (m_xAxisRangeMode == AxisRangeMode::Auto) {
        m_plot->xAxis->setRange(minX - rangeX * 0.02, maxX + rangeX * 0.02);
    } else {
        m_plot->xAxis->setRange(m_manualXMin, m_manualXMax);
    }

    if (m_yAxisRangeMode == AxisRangeMode::Auto) {
        double minY = *std::min_element(m_yData.constBegin(), m_yData.constEnd());
        double maxY = *std::max_element(m_yData.constBegin(), m_yData.constEnd());
        double yPadding = (maxY - minY) * 0.02;
        if (yPadding < 1.0) {
            yPadding = 1.0;
        }
        m_plot->yAxis->setRange(minY - yPadding, maxY + yPadding);
    } else {
        m_plot->yAxis->setRange(m_manualYMin, m_manualYMax);
    }
}

void SpectrumViewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dataValid || m_xData.isEmpty()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    double dataX = widgetToDataX(event->pos().x());
    int idx = qRound(dataX);
    if (idx < 0) idx = 0;
    if (idx >= m_xData.size()) idx = m_xData.size() - 1;
    double snappedX = m_xData[idx];
    double snappedY = m_yData[idx];

    updateCursor(snappedX, snappedY);

    emit cursorPosition(snappedX, snappedY);

    QWidget::mouseMoveEvent(event);
}

void SpectrumViewWidget::leaveEvent(QEvent *event)
{
    m_cursorActive = false;
    m_cursorLine->setVisible(false);
    m_cursorLabel->setVisible(false);
    m_plot->layer(QLatin1String("overlay"))->replot();

    emit cursorLeft();

    QWidget::leaveEvent(event);
}

void SpectrumViewWidget::resizeEvent(QResizeEvent *event)
{
    if (m_plot) {
        m_plot->resize(size());
        // 读数框是按像素定位的，窗口一变就得重新贴回绘图区左上角
        if (m_cursorActive) {
            applyCursor(m_lastCursorX, intensityAt(m_lastCursorX));
        }
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
                resetZoom();
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

void SpectrumViewWidget::updateCursor(double x, double y)
{
    if (!m_dataValid) {
        return;
    }

    // 记下光标位置：新一帧到达时数据变了但鼠标没动，靠它重新取当前值
    m_lastCursorX = x;
    m_cursorActive = true;

    applyCursor(x, y);
    m_plot->layer(QLatin1String("overlay"))->replot();

    emit cursorPosition(x, y);
}

// 只改 overlay 上的图元和文字。数据刷新那条路也要用它，但那边已经把
// 整幅图排进重绘队列了，不能再单独让 overlay 同步重绘一遍
void SpectrumViewWidget::applyCursor(double x, double y)
{
    double minY = m_plot->yAxis->range().lower;
    double maxY = m_plot->yAxis->range().upper;

    m_cursorLine->start->setCoords(x, minY);
    m_cursorLine->end->setCoords(x, maxY);
    m_cursorLine->setVisible(true);

    QString labelText = QString("X: %1\nI: %2")
        .arg(x, 0, 'f', 1)
        .arg(y, 0, 'f', 0);

    m_cursorLabel->setText(labelText);

    // 按像素贴住绘图区左上角。原来是按数据坐标偏 5 个单位，量程一变宽这点偏移
    // 就不剩几个像素了，读数框会紧贴左边框
    const QRect area = m_plot->axisRect()->rect();
    m_cursorLabel->position->setPixelPosition(area.topLeft()
                                              + QPointF(kCursorLabelInset, kCursorLabelInset));
    m_cursorLabel->setVisible(true);
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

void SpectrumViewWidget::resetZoom()
{
    if (!m_dataValid || m_xData.isEmpty()) {
        return;
    }

    m_userHasZoomed = false;
    applyAxisRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}