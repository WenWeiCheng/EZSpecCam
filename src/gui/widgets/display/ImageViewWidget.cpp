#include "ImageViewWidget.h"

#include <QVBoxLayout>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QTimer>
#include "../../qcustomplot.h"

ImageViewWidget::ImageViewWidget(QWidget *parent)
    : QWidget(parent)
    , m_plot(nullptr)
    , m_colorMap(nullptr)
    , m_imageValid(false)
    , m_resizeTimer(new QTimer(this))
    , m_rubberBand(nullptr)
{
    m_plot = new QCustomPlot(this);
    m_rubberBand = new QRubberBand(QRubberBand::Rectangle, m_plot);

    m_plot->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_plot->installEventFilter(this);

    connect(m_resizeTimer, &QTimer::timeout, this, &ImageViewWidget::onResizeTimeout);

    setupPlot();
}

ImageViewWidget::~ImageViewWidget() = default;

void ImageViewWidget::setupPlot()
{
    m_colorMap = new QCPColorMap(m_plot->xAxis, m_plot->yAxis);
    m_colorMap->setInterpolate(false);
    m_colorMap->setTightBoundary(false);

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

    updatePlotGeometry();
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

    if (!m_userHasZoomed) {
        m_plot->xAxis->setRange(0, origWidth);
        m_plot->yAxis->setRange(0, origHeight);
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
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

void ImageViewWidget::setInteractionMode(InteractionMode mode)
{
    if (m_interactionMode == mode) {
        return;
    }

    m_interactionMode = mode;

    if (m_rubberBand->isVisible()) {
        m_rubberBand->hide();
    }
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
    m_currentCrosshairPos = QPointF();
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    emit crosshairsCleared();
}

void ImageViewWidget::addCrosshair(int x, int y)
{
    if (!m_imageValid || m_originalImage.isNull()) {
        return;
    }

    clearCrosshairs();

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
    m_currentCrosshairPos = QPointF(x, y);
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    int value = pixelValue(x, y);
    emit crosshairMoved(QPointF(x, y), value);
}

void ImageViewWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_imageValid || m_originalImage.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }

    // Only process crosshair if click is within m_plot bounds
    if (!m_plot->geometry().contains(event->pos())) {
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

void ImageViewWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_imageValid || m_originalImage.isNull()) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (m_crosshairs.isEmpty()) {
        QWidget::keyPressEvent(event);
        return;
    }

    int x = static_cast<int>(m_currentCrosshairPos.x());
    int y = static_cast<int>(m_currentCrosshairPos.y());

    switch (event->key()) {
        case Qt::Key_Up:
            y -= 1;
            break;
        case Qt::Key_Down:
            y += 1;
            break;
        case Qt::Key_Left:
            x -= 1;
            break;
        case Qt::Key_Right:
            x += 1;
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= m_originalImage.width()) x = m_originalImage.width() - 1;
    if (y >= m_originalImage.height()) y = m_originalImage.height() - 1;

    auto &pair = m_crosshairs.first();
    pair.first->start->setCoords(x, 0);
    pair.first->end->setCoords(x, m_originalImage.height());
    pair.second->start->setCoords(0, y);
    pair.second->end->setCoords(m_originalImage.width(), y);

    m_currentCrosshairPos = QPointF(x, y);
    m_plot->replot(QCustomPlot::rpQueuedReplot);

    int value = pixelValue(x, y);
    emit crosshairMoved(QPointF(x, y), value);

    event->accept();
}

void ImageViewWidget::leaveEvent(QEvent *event)
{
    setToolTip(QString());
    QWidget::leaveEvent(event);
}

void ImageViewWidget::resizeEvent(QResizeEvent *event)
{
    if (m_plot) {
        QTimer::singleShot(0, this, [this]() {
            updatePlotGeometry();
        });
    }

    if (event) {
        if (m_resizeTimer->isActive()) {
            m_resizeTimer->stop();
        }
        m_resizeTimer->setSingleShot(true);
        m_resizeTimer->start(100);
    }

    QWidget::resizeEvent(event);
}

void ImageViewWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, [this]() {
        updatePlotGeometry();
        update();
        m_plot->replot();
        qApp->processEvents();
        updateDisplayData();
    });
    m_resizeTimer->start(50);
}

void ImageViewWidget::updatePlotGeometry()
{
    if (!m_plot) {
        return;
    }

    QSize availableSize = size();
    if (availableSize.width() <= 0 || availableSize.height() <= 0) {
        return;
    }

    int margin = 5;

    double xRange = m_plot->xAxis->range().upper - m_plot->xAxis->range().lower;
    double yRange = m_plot->yAxis->range().upper - m_plot->yAxis->range().lower;
    double currentAspect = (yRange > 0) ? (xRange / yRange) : 1.0;

    int availWidth = availableSize.width() - margin * 2;
    int availHeight = availableSize.height() - margin * 2;

    int plotWidth, plotHeight;
    if (currentAspect > static_cast<double>(availWidth) / availHeight) {
        plotWidth = availWidth;
        plotHeight = static_cast<int>(availWidth / currentAspect);
    } else {
        plotHeight = availHeight;
        plotWidth = static_cast<int>(availHeight * currentAspect);
    }

    if (plotWidth <= 0) plotWidth = 1;
    if (plotHeight <= 0) plotHeight = 1;

    int totalWidth = plotWidth + margin * 2;
    int totalHeight = plotHeight + margin * 2;

    int x = (availableSize.width() - totalWidth) / 2;
    int y = (availableSize.height() - totalHeight) / 2;
    m_plot->setGeometry(x, y, totalWidth, totalHeight);

    m_plot->axisRect()->setMargins(QMargins(margin, margin, margin, margin));
    m_plot->axisRect()->setMinimumSize(plotWidth, plotHeight);
    m_plot->axisRect()->setMaximumSize(plotWidth, plotHeight);
}

void ImageViewWidget::onResizeTimeout()
{
    updatePlotGeometry();
    update();
    m_plot->replot();
    qApp->processEvents();
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
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                if (me->modifiers() & Qt::ControlModifier) {
                    mousePressEvent(me);
                } else {
                    QRect axisRect = m_plot->axisRect()->rect();
                    if (axisRect.contains(me->pos()) && m_interactionMode == InteractionMode::Zoom) {
                        m_rubberBandOrigin = me->pos();
                        m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, QSize()));
                        m_rubberBand->show();
                    } else if (m_interactionMode == InteractionMode::Crosshair) {
                        mousePressEvent(me);
                    }
                }
                return true;
            } else if (me->button() == Qt::RightButton) {
                if (me->modifiers() & Qt::ControlModifier) {
                    mousePressEvent(me);
                } else if (m_interactionMode == InteractionMode::Zoom) {
                    resetZoomToFit();
                } else if (m_interactionMode == InteractionMode::Crosshair) {
                    mousePressEvent(me);
                }
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_rubberBand->isVisible()) {
                m_rubberBand->setGeometry(QRect(m_rubberBandOrigin, me->pos()).normalized());
                return true;
            }
            if (m_interactionMode == InteractionMode::Crosshair) {
                mouseMoveEvent(me);
            }
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_rubberBand->isVisible()) {
                m_rubberBand->hide();
                if (me->button() == Qt::LeftButton) {
                    if (m_imageValid && !m_originalImage.isNull()) {
                        QRectF selectionRect = QRectF(m_rubberBandOrigin, me->pos()).normalized();

                        double x1 = m_plot->xAxis->pixelToCoord(selectionRect.left());
                        double x2 = m_plot->xAxis->pixelToCoord(selectionRect.right());
                        double y1 = m_plot->yAxis->pixelToCoord(selectionRect.bottom());
                        double y2 = m_plot->yAxis->pixelToCoord(selectionRect.top());
                        if (qAbs(x2 - x1) > 0 && qAbs(y2 - y1) > 0) {
                            m_plot->xAxis->setRange(x1, x2);
                            m_plot->yAxis->setRange(y1, y2);
                            m_plot->replot(QCustomPlot::rpQueuedReplot);
                            updatePlotGeometry();
                            m_userHasZoomed = true;
                        }
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

QVector<double> ImageViewWidget::extractRowAsVector(int y) const
{
    QVector<double> data;
    if (!m_imageValid || m_originalImage.isNull()) {
        return data;
    }

    if (y < 0 || y >= m_originalImage.height()) {
        return data;
    }

    int width = m_originalImage.width();
    data.reserve(width);

    if (m_originalImage.format() == QImage::Format_Grayscale16) {
        const uchar *bits = m_originalImage.constBits();
        const ushort *gray16 = reinterpret_cast<const ushort *>(bits + y * m_originalImage.bytesPerLine());
        for (int x = 0; x < width; ++x) {
            data.append(static_cast<double>(gray16[x]));
        }
    } else if (m_originalImage.format() == QImage::Format_Grayscale8) {
        const uchar *gray8 = m_originalImage.constBits() + y * m_originalImage.bytesPerLine();
        for (int x = 0; x < width; ++x) {
            data.append(static_cast<double>(gray8[x]));
        }
    } else {
        for (int x = 0; x < width; ++x) {
            QRgb pixel = m_originalImage.pixel(x, y);
            data.append(static_cast<double>(qGray(pixel)));
        }
    }

    return data;
}

void ImageViewWidget::resetZoomToFit()
{
    if (!m_imageValid || m_originalImage.isNull()) {
        return;
    }

    m_plot->xAxis->setRange(0, m_originalImage.width());
    m_plot->yAxis->setRange(0, m_originalImage.height());
    m_plot->replot(QCustomPlot::rpQueuedReplot);
    updatePlotGeometry();
    m_userHasZoomed = false;
}

QVector<double> ImageViewWidget::extractColumnAsVector(int x) const
{
    QVector<double> data;
    if (!m_imageValid || m_originalImage.isNull()) {
        return data;
    }

    if (x < 0 || x >= m_originalImage.width()) {
        return data;
    }

    int height = m_originalImage.height();
    data.reserve(height);

    if (m_originalImage.format() == QImage::Format_Grayscale16) {
        const uchar *bits = m_originalImage.constBits();
        const ushort *gray16 = reinterpret_cast<const ushort *>(bits);
        for (int y = 0; y < height; ++y) {
            data.append(static_cast<double>(gray16[y * m_originalImage.bytesPerLine() / 2 + x]));
        }
    } else if (m_originalImage.format() == QImage::Format_Grayscale8) {
        const uchar *gray8 = m_originalImage.constBits();
        for (int y = 0; y < height; ++y) {
            data.append(static_cast<double>(gray8[y * m_originalImage.bytesPerLine() + x]));
        }
    } else {
        for (int y = 0; y < height; ++y) {
            QRgb pixel = m_originalImage.pixel(x, y);
            data.append(static_cast<double>(qGray(pixel)));
        }
    }

    return data;
}