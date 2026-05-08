#include "ImageViewWidget.h"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QDebug>
#include <QTimer>
#include "../../qcustomplot.h"

ImageViewWidget::ImageViewWidget(QWidget *parent)
    : QWidget(parent)
    , m_plot(nullptr)
    , m_colorMap(nullptr)
    , m_imageValid(false)
    , m_resizeTimer(new QTimer(this))
{
    m_plot = new QCustomPlot(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_plot);

    m_plot->installEventFilter(this);

    connect(m_resizeTimer, &QTimer::timeout, this, &ImageViewWidget::onResizeTimeout);

    setupPlot();
}

ImageViewWidget::~ImageViewWidget()
{
    clearCrosshairs();
}

void ImageViewWidget::setupPlot()
{
    m_colorMap = new QCPColorMap(m_plot->xAxis, m_plot->yAxis);
    m_colorMap->setInterpolate(false);
    m_colorMap->setTightBoundary(false);

    QCPColorScale *colorScale = new QCPColorScale(m_plot);
    m_plot->plotLayout()->addElement(0, 1, colorScale);
    m_colorMap->setColorScale(colorScale);
    colorScale->setLabel("Intensity");

    QCPColorGradient gradient;
    gradient.setLevelCount(256);
    gradient.setColorStopAt(0, QColor(0, 0, 0));
    gradient.setColorStopAt(0.5, QColor(128, 128, 128));
    gradient.setColorStopAt(1, QColor(255, 255, 255));
    m_colorMap->setGradient(gradient);

    m_plot->xAxis->setLabel("X (pixels)");
    m_plot->yAxis->setLabel("Y (pixels)");
    m_plot->xAxis->setRange(0, 100);
    m_plot->yAxis->setRange(0, 100);
    m_plot->yAxis->setRangeReversed(true);

    m_plot->setInteractions(QCP::iSelectItems | QCP::iSelectPlottables);
    m_plot->setMouseTracking(true);
    m_plot->axisRect()->setAutoMargins(QCP::msNone);
    if (m_axesVisible) {
        m_plot->axisRect()->setMargins(QMargins(80, 20, 20, 50));
    } else {
        m_plot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
    }

    // m_plot->setOpenGl(true);
    m_plot->setNoAntialiasingOnDrag(true);

    m_axesVisible = false;
    m_plot->xAxis->setVisible(false);
    m_plot->yAxis->setVisible(false);
    m_plot->axisRect()->setMargins(QMargins(3, 3, 3, 3));

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void ImageViewWidget::setImage(const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    m_originalImage = image;
    m_currentImage = image;
    m_imageValid = true;

    m_displayImage = QImage();

    updateDisplayData();
}

void ImageViewWidget::updateColorMap(const QImage &image)
{
    if (image.isNull()) {
        return;
    }

    const int origWidth = m_originalImage.width();
    const int origHeight = m_originalImage.height();

    const int dataWidth = image.width();
    const int dataHeight = image.height();

    QCPColorMapData *newMapData = new QCPColorMapData(dataWidth, dataHeight,
                                                       QCPRange(0, origWidth),
                                                       QCPRange(0, origHeight));

    if (image.format() == QImage::Format_Grayscale16) {
        for (int y = 0; y < dataHeight; ++y) {
            const quint16 *sourceLine = reinterpret_cast<const quint16 *>(
                image.constBits() + y * image.bytesPerLine());
            for (int x = 0; x < dataWidth; ++x) {
                newMapData->setCell(x, y, static_cast<double>(sourceLine[x]));
            }
        }
    } else {
        for (int y = 0; y < dataHeight; ++y) {
            for (int x = 0; x < dataWidth; ++x) {
                QRgb pixel = image.pixel(x, y);
                newMapData->setCell(x, y, static_cast<double>(qGray(pixel)));
            }
        }
    }

    m_colorMap->setData(newMapData, false);

    m_colorMap->rescaleDataRange();
    applyColorScaleMode();

    m_plot->xAxis->setRange(0, origWidth);
    m_plot->yAxis->setRange(0, origHeight);
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void ImageViewWidget::applyColorScaleMode()
{
    switch (m_colorScaleMode) {
        case ColorScaleMode::Auto:
            m_colorMap->rescaleDataRange();
            break;
        case ColorScaleMode::Fixed8Bit:
            m_colorMap->setDataRange(QCPRange(0, 255));
            break;
        case ColorScaleMode::Fixed16Bit:
            m_colorMap->setDataRange(QCPRange(0, 65535));
            break;
    }
}

void ImageViewWidget::setColorScaleMode(ColorScaleMode mode)
{
    if (m_colorScaleMode == mode) {
        return;
    }

    m_colorScaleMode = mode;

    if (m_imageValid && !m_currentImage.isNull()) {
        applyColorScaleMode();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void ImageViewWidget::setAxesVisible(bool visible)
{
    if (m_axesVisible == visible) {
        return;
    }

    m_axesVisible = visible;
    m_plot->xAxis->setVisible(visible);
    m_plot->yAxis->setVisible(visible);

    if (m_axesVisible) {
        m_plot->axisRect()->setMargins(QMargins(65, 10, 20, 40));
    } else {
        m_plot->axisRect()->setMargins(QMargins(3, 3, 3, 3));
    }

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

QImage ImageViewWidget::image() const
{
    return m_currentImage;
}

bool ImageViewWidget::hasImage() const
{
    return m_imageValid && !m_originalImage.isNull();
}

QList<QPointF> ImageViewWidget::crosshairPositions() const
{
    QList<QPointF> positions;
    for (const auto &pair : m_crosshairs) {
        QPointF pos(pair.first->start->key(), pair.first->start->value());
        positions.append(pos);
    }
    return positions;
}

int ImageViewWidget::crosshairCount() const
{
    return m_crosshairs.size();
}

void ImageViewWidget::clearCrosshairs()
{
    for (auto &pair : m_crosshairs) {
        m_plot->removeItem(pair.first);
        m_plot->removeItem(pair.second);
    }
    m_crosshairs.clear();
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    emit crosshairsCleared();
}

void ImageViewWidget::addCrosshair(int x, int y)
{
    if (!m_imageValid || m_originalImage.isNull()) {
        return;
    }

    QPen crosshairPen(QColor(255, 0, 0, 180));
    crosshairPen.setWidth(1);
    crosshairPen.setStyle(Qt::DashLine);

    QCPItemLine *verticalLine = new QCPItemLine(m_plot);
    verticalLine->setPen(crosshairPen);
    verticalLine->setSelectable(false);
    verticalLine->start->setCoords(x, 0);
    verticalLine->end->setCoords(x, m_originalImage.height());

    QCPItemLine *horizontalLine = new QCPItemLine(m_plot);
    horizontalLine->setPen(crosshairPen);
    horizontalLine->setSelectable(false);
    horizontalLine->start->setCoords(0, y);
    horizontalLine->end->setCoords(m_originalImage.width(), y);

    m_crosshairs.append(qMakePair(verticalLine, horizontalLine));
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    emit crosshairAdded(QPointF(x, y));
}

void ImageViewWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_imageValid || m_originalImage.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }

    QPointF imageCoords = widgetToImageCoords(event->pos().x(), event->pos().y());
    int x = static_cast<int>(imageCoords.x());
    int y = static_cast<int>(imageCoords.y());

    if (x >= 0 && x < m_originalImage.width() &&
        y >= 0 && y < m_originalImage.height()) {

        if (event->button() == Qt::LeftButton) {
            addCrosshair(x, y);
        } else if (event->button() == Qt::RightButton) {
            clearCrosshairs();
        }
    }

    QWidget::mousePressEvent(event);
}

void ImageViewWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_imageValid || m_originalImage.isNull()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    QPointF imageCoords = widgetToImageCoords(event->pos().x(), event->pos().y());
    int x = static_cast<int>(imageCoords.x());
    int y = static_cast<int>(imageCoords.y());

    if (x >= 0 && x < m_originalImage.width() &&
        y >= 0 && y < m_originalImage.height()) {

        int value = pixelValue(x, y);

        QString tooltip = QString("X: %1, Y: %2, Value: %3")
                          .arg(x)
                          .arg(y)
                          .arg(value);

        setToolTip(tooltip);
        emit pixelInfo(x, y, value);
    } else {
        setToolTip(QString());
    }

    QWidget::mouseMoveEvent(event);
}

void ImageViewWidget::leaveEvent(QEvent *event)
{
    setToolTip(QString());
    QWidget::leaveEvent(event);
}

void ImageViewWidget::resizeEvent(QResizeEvent *event)
{
    if (m_plot) {
        m_plot->resize(size());
    }

    if (m_resizeTimer->isActive()) {
        m_resizeTimer->stop();
    }
    m_resizeTimer->setSingleShot(true);
    m_resizeTimer->start(100);

    QWidget::resizeEvent(event);
}

void ImageViewWidget::onResizeTimeout()
{
    QSize currentSize = QSize(m_plot->axisRect()->width(), m_plot->axisRect()->height());

    if (currentSize != m_lastViewportSize) {
        m_lastViewportSize = currentSize;
        updateDisplayData();
    }
}

bool ImageViewWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_plot) {
        if (event->type() == QEvent::MouseButtonPress) {
            mousePressEvent(static_cast<QMouseEvent *>(event));
            return true;
        } else if (event->type() == QEvent::MouseMove) {
            mouseMoveEvent(static_cast<QMouseEvent *>(event));
            return true;
        } else if (event->type() == QEvent::Leave) {
            leaveEvent(event);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

int ImageViewWidget::pixelValue(int x, int y) const
{
    if (!m_imageValid || m_originalImage.isNull()) {
        return 0;
    }

    if (x < 0 || x >= m_originalImage.width() ||
        y < 0 || y >= m_originalImage.height()) {
        return 0;
    }

    if (m_originalImage.format() == QImage::Format_Grayscale16) {
        const uchar *bits = m_originalImage.constBits();
        const ushort *gray16 = reinterpret_cast<const ushort *>(bits + y * m_originalImage.bytesPerLine());
        return static_cast<int>(gray16[x]);
    } else {
        QRgb pixel = m_originalImage.pixel(x, y);
        return qGray(pixel);
    }
}

QPointF ImageViewWidget::widgetToImageCoords(int widgetX, int widgetY) const
{
    if (!m_plot) {
        return QPointF(0, 0);
    }

    double x = m_plot->xAxis->pixelToCoord(widgetX);
    double y = m_plot->yAxis->pixelToCoord(widgetY);

    return QPointF(x, y);
}

void ImageViewWidget::calculateDownsampleFactors()
{
    if (!m_imageValid || m_originalImage.isNull()) {
        m_downsampleX = 1;
        m_downsampleY = 1;
        return;
    }

    if (!m_downsamplingEnabled) {
        m_downsampleX = 1;
        m_downsampleY = 1;
        return;
    }

    int viewWidth = m_plot->axisRect()->width();
    int viewHeight = m_plot->axisRect()->height();

    if (viewWidth <= 0 || viewHeight <= 0) {
        viewWidth = m_plot->width() - 100;
        viewHeight = m_plot->height() - 70;
    }

    if (viewWidth <= 0 || viewHeight <= 0) {
        m_downsampleX = 1;
        m_downsampleY = 1;
        return;
    }

    int imgWidth = m_originalImage.width();
    int imgHeight = m_originalImage.height();

    m_downsampleX = qMax(1, imgWidth / (viewWidth));
    m_downsampleY = qMax(1, imgHeight / (viewHeight));
}

QImage ImageViewWidget::downsampleImage(const QImage &source, int factorX, int factorY)
{
    if (factorX <= 1 && factorY <= 1) {
        return source;
    }

    int newWidth = source.width() / factorX;
    int newHeight = source.height() / factorY;

    if (newWidth <= 0) newWidth = 1;
    if (newHeight <= 0) newHeight = 1;

    return source.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::FastTransformation);
}

void ImageViewWidget::updateDisplayData()
{
    if (!m_imageValid || m_originalImage.isNull()) {
        return;
    }

    int prevDownsampleX = m_downsampleX;
    int prevDownsampleY = m_downsampleY;

    calculateDownsampleFactors();

    if (prevDownsampleX == m_downsampleX &&
        prevDownsampleY == m_downsampleY &&
        !m_displayImage.isNull()) {
        return;
    }

    m_originalPixelCount = m_originalImage.width() * m_originalImage.height();

    m_displayImage = downsampleImage(m_originalImage, m_downsampleX, m_downsampleY);
    m_displayPixelCount = m_displayImage.width() * m_displayImage.height();

    updateColorMap(m_displayImage);
}

bool ImageViewWidget::isDownsamplingEnabled() const
{
    return m_downsamplingEnabled;
}

void ImageViewWidget::setDownsamplingEnabled(bool enabled)
{
    if (m_downsamplingEnabled == enabled) {
        return;
    }

    m_downsamplingEnabled = enabled;

    if (m_imageValid && !m_originalImage.isNull()) {
        updateDisplayData();
    }
}