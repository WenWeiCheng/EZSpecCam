#ifndef IMAGEVIEWWIDGET_H
#define IMAGEVIEWWIDGET_H

#include <QWidget>
#include <QImage>
#include <QMouseEvent>
#include <QList>
#include <QPair>
#include <QPointF>
#include <QTimer>

class QCustomPlot;
class QCPColorMap;
class QCPItemLine;
class QCPItemText;

class ImageViewWidget : public QWidget
{
    Q_OBJECT
public:
    enum class ColorScaleMode {
        Auto,
        Fixed8Bit,
        Fixed16Bit
    };
    Q_ENUM(ColorScaleMode)

    explicit ImageViewWidget(QWidget *parent = nullptr);
    ~ImageViewWidget() override;

    void setImage(const QImage &image);
    QImage image() const;
    bool hasImage() const;

    ColorScaleMode colorScaleMode() const { return m_colorScaleMode; }
    void setColorScaleMode(ColorScaleMode mode);

    bool axesVisible() const { return m_axesVisible; }
    void setAxesVisible(bool visible);

    QList<QPointF> crosshairPositions() const;
    int crosshairCount() const;
    void clearCrosshairs();
    void addCrosshair(int x, int y);

    bool isDownsamplingEnabled() const;
    void setDownsamplingEnabled(bool enabled);

    int pixelValue(int x, int y) const;

signals:
    void crosshairAdded(const QPointF &position);
    void crosshairMoved(const QPointF &position, int value);
    void crosshairsCleared();
    void pixelInfo(int x, int y, int value);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onResizeTimeout();

private:
    void setupPlot();
    void updateColorMap(const QImage &image);
    void applyColorScaleMode();
    QPointF widgetToImageCoords(int widgetX, int widgetY) const;
    void updateDisplayData();
    void calculateDownsampleFactors();
    QImage downsampleImage(const QImage &source, int factorX, int factorY);

    QCustomPlot *m_plot;
    QCPColorMap *m_colorMap;
    QImage m_currentImage;
    QList<QPair<QCPItemLine *, QCPItemLine *>> m_crosshairs;
    QPointF m_currentCrosshairPos;
    bool m_imageValid;
    ColorScaleMode m_colorScaleMode = ColorScaleMode::Auto;
    bool m_axesVisible = false;

    QImage m_originalImage;
    QImage m_displayImage;

    int m_downsampleX = 1;
    int m_downsampleY = 1;
    bool m_downsamplingEnabled = true;

    int m_originalPixelCount = 0;
    int m_displayPixelCount = 0;

    QTimer *m_resizeTimer;
    QSize m_lastViewportSize;
};

#endif // IMAGEVIEWWIDGET_H