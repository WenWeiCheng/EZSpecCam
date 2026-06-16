#ifndef IMAGEVIEWWIDGET_H
#define IMAGEVIEWWIDGET_H

#include <QWidget>
#include <QImage>
#include <QMouseEvent>
#include <QShowEvent>
#include <QList>
#include <QPair>
#include <QPointF>
#include <QTimer>
#include <QRubberBand>

class QCustomPlot;
class QCPColorMap;
class QCPColorScale;
class QCPItemLine;
class QCPItemText;

class ImageViewWidget : public QWidget
{
    Q_OBJECT
public:
    enum class FitMode {
        KeepAspectRatio,
        FillWindow
    };
    Q_ENUM(FitMode)

    enum class ColorMap {
        Grayscale,
        Hot,
        Cold,
        Night,
        Candy,
        Geography,
        Ion,
        Thermal,
        Polar,
        Spectrum,
        Jet
    };
    Q_ENUM(ColorMap)

    enum class ColorScaleMode {
        Auto,
        Fixed8Bit,
        Fixed16Bit
    };
    Q_ENUM(ColorScaleMode)

    enum class IntensityScaleType {
        Linear,
        Log
    };
    Q_ENUM(IntensityScaleType)

    explicit ImageViewWidget(QWidget *parent = nullptr);
    ~ImageViewWidget() override;

    void setImage(const QImage &image);
    QImage image() const;
    bool hasImage() const;

    ColorMap colorMap() const { return m_colorMapPreset; }
    void setColorMap(ColorMap map);

    FitMode fitMode() const { return m_fitMode; }
    void setFitMode(FitMode mode);

    ColorScaleMode colorScaleMode() const { return m_colorScaleMode; }
    void setColorScaleMode(ColorScaleMode mode);

    IntensityScaleType intensityScaleType() const { return m_intensityScaleType; }
    void setIntensityScaleType(IntensityScaleType type);

    bool axesVisible() const { return m_axesVisible; }
    void setAxesVisible(bool visible);

    bool isColorScaleVisible() const { return m_colorScaleVisible; }
    void setColorScaleVisible(bool visible);

    QList<QPointF> crosshairPositions() const;
    int crosshairCount() const;
    void clearCrosshairs();
    void addCrosshair(int x, int y);

    bool isDownsamplingEnabled() const;
    void setDownsamplingEnabled(bool enabled);

    int pixelValue(int x, int y) const;

    QVector<double> extractRowAsVector(int y) const;
    QVector<double> extractColumnAsVector(int x) const;

signals:
    void crosshairAdded(const QPointF &position);
    void crosshairMoved(const QPointF &position, int value);
    void crosshairsCleared();
    void pixelInfo(int x, int y, int value);
    void overexposureDetected(QPoint position);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onResizeTimeout();

private:
    void setupPlot();
    void updateColorMap(const QImage &image);
    void checkOverexposure(const QImage &image);
    void applyColorMap();
    void applyColorScaleMode();
    void setupColorScalePlot();
    QPointF widgetToImageCoords(int widgetX, int widgetY) const;
    void updateDisplayData();
    void calculateDownsampleFactors();
    QImage downsampleImage(const QImage &source, int factorX, int factorY);
    void resetZoomToFit();
    void updatePlotGeometry();

    QCustomPlot *m_plot;
    QCPColorMap *m_colorMap;
    QCustomPlot *m_colorScalePlot = nullptr;
    QCPColorScale *m_colorScale = nullptr;
    QImage m_currentImage;
    QList<QPair<QCPItemLine *, QCPItemLine *>> m_crosshairs;
    QPointF m_currentCrosshairPos;
    bool m_imageValid;
    ColorMap m_colorMapPreset = ColorMap::Grayscale;
    FitMode m_fitMode = FitMode::KeepAspectRatio;
    ColorScaleMode m_colorScaleMode = ColorScaleMode::Auto;
    IntensityScaleType m_intensityScaleType = IntensityScaleType::Linear;
    bool m_axesVisible = false;
    bool m_colorScaleVisible = true;

    QImage m_originalImage;
    QImage m_displayImage;

    int m_downsampleX = 1;
    int m_downsampleY = 1;
    bool m_downsamplingEnabled = true;

    int m_originalPixelCount = 0;
    int m_displayPixelCount = 0;

    QTimer *m_resizeTimer;
    QSize m_lastViewportSize;

    QRubberBand *m_rubberBand = nullptr;
    QPoint m_rubberBandOrigin;
    bool m_userHasZoomed = false;
};

#endif // IMAGEVIEWWIDGET_H