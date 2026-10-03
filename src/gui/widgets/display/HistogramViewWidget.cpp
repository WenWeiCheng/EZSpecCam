#include "HistogramViewWidget.h"
#include "../../Theme.h"
#include "../../qcustomplot.h"

#include <QShowEvent>
#include <QVBoxLayout>
#include <QtGlobal>

namespace {
// 线性轴的柱子从 0 起画。对数轴上 0 画不出来（log10(0) 是 -inf，
// QCPBars::getBarRect 拿它算矩形会得到废掉的几何），所以对数轴下
// 把柱子底挪到 kLogBase，计数为 0 的箱自然画成零高度，也就是不画
constexpr double kLogBase = 1.0;
// 对数轴纵轴下界
constexpr double kLogRangeLower = 1.0;
// 框选缩放后至少保留几个箱。再小的话横坐标的刻度间距会掉到 1 以下，
// 刻度位置就成了 12.1、12.2 这种，分数标签和重复标签一起冒出来
constexpr int kMinZoomBins = 4;
} // namespace

HistogramViewWidget::HistogramViewWidget(QWidget *parent)
    : QWidget(parent)
    , m_plot(nullptr)
    , m_bars(nullptr)
    , m_xAxisLabel("Value")
    , m_yAxisLabel("Pixel Count")
{
    m_plot = new QCustomPlot(this);

    m_axisSelection = new QRubberBand(QRubberBand::Rectangle, m_plot);
    m_plot->installEventFilter(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_plot);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setupPlot();
}

HistogramViewWidget::~HistogramViewWidget() = default;

void HistogramViewWidget::setupPlot()
{
    m_bars = new QCPBars(m_plot->xAxis, m_plot->yAxis);
    // 箱宽用数据坐标给定：这样放大绘图区时箱子跟着一起变宽，
    // 而不会像默认的按绘图区比例那样把 256 根柱子糊成一块
    m_bars->setWidthType(QCPBars::wtPlotCoords);
    m_bars->setWidth(1.0);
    // 相邻箱子共用边线，不描边才不会在密的时候显出一条条竖线
    m_bars->setPen(Qt::NoPen);
    m_bars->setBaseValue(0.0);

    m_plot->axisRect()->setupFullAxesBox(true);
    m_plot->xAxis->setLabel(m_xAxisLabel);
    m_plot->yAxis->setLabel(m_yAxisLabel);

    // 横坐标是像素值，标签必须是整数。QCPAxis::setNumberFormat 只认首字符
    // 是 eEfgG 的格式码，传 "%.0f" 会被它静默拒绝（只打一行 qDebug），
    // 所以这里给 "f" 再单独把精度压到 0。
    // 放在 setupFullAxesBox 之前：那一刻它会把刻度器复制一份给顶部轴
    m_plot->xAxis->setNumberFormat("f");
    m_plot->xAxis->setNumberPrecision(0);

    applyTheme();

    connect(Theme::instance(), &Theme::themeChanged, this, &HistogramViewWidget::applyTheme);
}

void HistogramViewWidget::applyTheme()
{
    const Theme::PlotColors &c = Theme::instance()->plotColors();
    Theme::instance()->applyToPlot(m_plot);
    m_bars->setBrush(QBrush(c.curve));
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void HistogramViewWidget::setHistogram(const QVector<double> &bins, const QVector<double> &counts)
{
    const int n = qMin(bins.size(), counts.size());

    m_binCenters = bins;
    m_counts = counts;
    m_binCenters.resize(n);
    m_counts.resize(n);
    m_hasData = n > 0;

    if (!m_hasData) {
        clearHistogram();
        return;
    }

    if (n >= 2) {
        m_binWidth = m_binCenters.at(1) - m_binCenters.at(0);
    } else {
        m_binWidth = 1.0;
    }
    if (m_binWidth <= 0.0) {
        m_binWidth = 1.0;
    }

    m_bars->setWidth(m_binWidth);
    m_bars->setData(m_binCenters, m_counts, true);

    applyAxisRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void HistogramViewWidget::clearHistogram()
{
    m_hasData = false;
    m_binCenters.clear();
    m_counts.clear();
    // 数据都清了还留着框选量程的话，下一帧进来时横坐标会卡在上一个图的范围内
    m_userHasZoomed = false;
    m_bars->data()->clear();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void HistogramViewWidget::setLogScale(bool enabled)
{
    if (m_logScale == enabled) {
        return;
    }

    m_logScale = enabled;
    applyScaleType();
    applyAxisRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void HistogramViewWidget::applyScaleType()
{
    if (m_logScale) {
        m_plot->yAxis->setScaleType(QCPAxis::stLogarithmic);
        m_plot->yAxis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTickerLog()));
        m_bars->setBaseValue(kLogBase);
    } else {
        m_plot->yAxis->setScaleType(QCPAxis::stLinear);
        m_plot->yAxis->setTicker(QSharedPointer<QCPAxisTicker>(new QCPAxisTicker()));
        m_bars->setBaseValue(0.0);
    }

    // 顶部那条轴共用左侧的刻度算法，否则对数刻度下它的刻度线会和左侧标签对不齐。
    // setupFullAxesBox 只在建框的那一刻复制过一次 ticker，之后换 ticker 必须再同步一次
    m_plot->yAxis2->setScaleType(m_plot->yAxis->scaleType());
    m_plot->yAxis2->setTicker(m_plot->yAxis->ticker());
}

void HistogramViewWidget::applyAxisRange()
{
    if (!m_hasData || m_binCenters.isEmpty()) {
        return;
    }

    // 框选缩放只动横坐标：缩放过之后新帧不再把横坐标拉回去，
    // 纵坐标则始终按当前数据自动——否则「只缩横轴」就名不副实了
    if (!m_userHasZoomed) {
        // 头尾各留半个箱宽，否则最外侧两根柱子会被绘图区边沿切掉
        const double half = m_binWidth * 0.5;
        m_plot->xAxis->setRange(m_binCenters.first() - half, m_binCenters.last() + half);
    }

    const double maxCount = *std::max_element(m_counts.constBegin(), m_counts.constEnd());
    if (m_logScale) {
        // 下界必须为正，否则对数轴刻度算不出来；上界同样留出余量，
        // 否则最高的箱子会顶到轴顶
        const double upper = (maxCount > 2.0) ? maxCount * 1.5 : 10.0;
        m_plot->yAxis->setRange(kLogRangeLower, upper);
    } else {
        m_plot->yAxis->setRange(0.0, (maxCount > 0.0) ? maxCount * 1.05 : 1.0);
    }
}

void HistogramViewWidget::resetZoom()
{
    if (!m_userHasZoomed) {
        return;
    }
    m_userHasZoomed = false;
    applyAxisRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

// 缩放是在绘图区里横向拖一条：选区是纵向的一条，高度占满整个绘图区，
// 只有宽度跟着拖动走。横坐标才是要放大的量，纵向拖没有意义
bool HistogramViewWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_plot) {
        return QWidget::eventFilter(obj, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            if (m_plot->axisRect()->rect().contains(me->pos())) {
                m_selectionOrigin = me->pos();
                m_axisSelection->setGeometry(selectionRect(m_selectionOrigin.x(), me->pos().x()));
                m_axisSelection->show();
                return true;
            }
        } else if (me->button() == Qt::RightButton) {
            resetZoom();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        if (m_axisSelection->isVisible()) {
            auto *me = static_cast<QMouseEvent *>(event);
            m_axisSelection->setGeometry(selectionRect(m_selectionOrigin.x(), me->pos().x()));
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        if (m_axisSelection->isVisible()) {
            m_axisSelection->hide();
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                applySelection(selectionRect(m_selectionOrigin.x(), me->pos().x()));
            }
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

// 选区矩形：纵向占满绘图区，横向只由拖动的起止决定，并夹在绘图区之内
QRect HistogramViewWidget::selectionRect(int fromX, int toX) const
{
    const QRect area = m_plot->axisRect()->rect();
    const int left = qBound(area.left(), qMin(fromX, toX), area.right());
    const int right = qBound(area.left(), qMax(fromX, toX), area.right());
    return QRect(left, area.top(), qMax(1, right - left), area.height());
}

void HistogramViewWidget::applySelection(const QRect &selection)
{
    if (!m_hasData) {
        return;
    }

    double x1 = m_plot->xAxis->pixelToCoord(selection.left());
    double x2 = m_plot->xAxis->pixelToCoord(selection.right());
    if (x1 > x2) {
        qSwap(x1, x2);
    }

    // 点了一下没拖动
    if (x2 - x1 <= 0.0) {
        return;
    }

    // 不许无限放大：拖得比最小宽度还窄时，按最小宽度居中放回去
    const double minWidth = minRangeWidth();
    if (x2 - x1 < minWidth) {
        const double center = (x1 + x2) * 0.5;
        x1 = center - minWidth * 0.5;
        x2 = center + minWidth * 0.5;
    }

    m_plot->xAxis->setRange(x1, x2);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    m_userHasZoomed = true;
}

double HistogramViewWidget::minRangeWidth() const
{
    return m_binWidth * kMinZoomBins;
}

void HistogramViewWidget::setXAxisLabel(const QString &label)
{
    m_xAxisLabel = label;
    m_plot->xAxis->setLabel(label);
}

void HistogramViewWidget::setYAxisLabel(const QString &label)
{
    m_yAxisLabel = label;
    m_plot->yAxis->setLabel(label);
}

void HistogramViewWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // 首次显示前后坐标轴的排版才最终定下来，这时候再套一次量程，
    // 免得窗口一开始就是空的
    applyAxisRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}
