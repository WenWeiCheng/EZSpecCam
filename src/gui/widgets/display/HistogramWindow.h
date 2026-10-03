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
    // 「每箱取值数」的默认值 1，即一个灰度值一箱：8 位图 256 个箱、16 位图
    // 65536 个箱，读单个灰度级上有多少像素不用再换算；想看整体分布时在窗口
    // 上把这个数调大即可。
    // 只在这里定一处：spin box 构造时按它 setValue，成员也按它初始化。曾经
    // 成员和常量各写一份 10，默认值改成 1 之后 spin 早就停在 1 上，
    // setValuesPerBin(1) 便不再发 valueChanged，成员留在 10 上——箱数和缩放
    // 下界一起错，而且不报错
    static constexpr int kDefaultValuesPerBin = 1;

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
    int m_valuesPerBin = kDefaultValuesPerBin;
    int m_overexposedCount = 0;
    int m_totalPixels = 0;
};

#endif // HISTOGRAMWINDOW_H
