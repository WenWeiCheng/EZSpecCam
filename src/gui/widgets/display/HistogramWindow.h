#ifndef HISTOGRAMWINDOW_H
#define HISTOGRAMWINDOW_H

#include <QDialog>
#include <QImage>
#include <QVector>

class QCheckBox;
class QLabel;
class QSpinBox;
class HistogramViewWidget;

class HistogramWindow : public QDialog
{
    Q_OBJECT
public:
    explicit HistogramWindow(QWidget *parent = nullptr);
    ~HistogramWindow() override;

    void setImage(const QImage &image);

    bool hasImage() const { return !m_image.isNull(); }
    QSize imageSize() const { return m_image.size(); }

    int overexposureThreshold() const { return m_threshold; }
    void setOverexposureThreshold(int value);

    //! 一个 bin 覆盖多少个灰度取值
    int valuesPerBin() const { return m_valuesPerBin; }
    void setValuesPerBin(int value);
    //! 按当前取值范围和每箱取值数算出来的实际箱数
    int binCount() const { return m_binCounts.size(); }

    bool isLogAxis() const;

    HistogramViewWidget *histogramWidget() const { return m_histogram; }
    QVector<double> binCenters() const { return m_binCenters; }
    QVector<double> binCounts() const { return m_binCounts; }

    int overexposedPixelCount() const { return m_overexposedCount; }
    int totalPixelCount() const { return m_totalPixels; }
    //! 当前图像格式的取值上限：8 位是 255，16 位是 65535
    int maxValue() const { return m_maxValue; }

private:
    void setupUi();
    void recompute();
    void updateOverexposureReadout();

    HistogramViewWidget *m_histogram = nullptr;
    QCheckBox *m_logAxisCheck = nullptr;
    QSpinBox *m_valuesPerBinSpin = nullptr;
    QSpinBox *m_thresholdSpin = nullptr;
    QLabel *m_overexposureLabel = nullptr;

    QImage m_image;
    QVector<double> m_binCenters;
    QVector<double> m_binCounts;
    //! 当前图像格式的取值上限：8 位是 255，16 位是 65535。
    //! 初值取 0（没有哪种格式能算出 0），好让第一张图一定走进 setImage()
    //! 的量程设置分支——否则 8 位图会因为「上限已经是 255」而整段跳过，
    //! 阈值控件就停在 65535 上，和真正参与判定的值对不上
    int m_maxValue = 0;
    int m_threshold = 255;
    //! 一个 bin 覆盖多少个灰度取值
    int m_valuesPerBin = 10;
    int m_overexposedCount = 0;
    int m_totalPixels = 0;
};

#endif // HISTOGRAMWINDOW_H
