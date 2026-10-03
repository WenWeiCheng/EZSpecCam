#include "HistogramWindow.h"
#include "HistogramViewWidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
// 16 位灰度图的取值上限
constexpr int kMaxValue16Bit = 65535;
// 「每箱取值数」的默认值和量程。默认 10 是因为 8 位图这样有 26 个箱，
// 曲线已经够顺；再密对读分布没什么帮助，反而把过曝数淹没在噪声里
constexpr int kDefaultValuesPerBin = 10;
} // namespace

HistogramWindow::HistogramWindow(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Histogram");
    setMinimumSize(640, 400);
    setModal(false);
    setupUi();
}

HistogramWindow::~HistogramWindow() = default;

void HistogramWindow::setupUi()
{
    m_histogram = new HistogramViewWidget(this);
    m_histogram->setXAxisLabel("Value");
    m_histogram->setYAxisLabel("Pixel Count");

    m_logAxisCheck = new QCheckBox("Log axis", this);

    m_valuesPerBinSpin = new QSpinBox(this);
    m_valuesPerBinSpin->setRange(1, kMaxValue16Bit);
    m_valuesPerBinSpin->setValue(kDefaultValuesPerBin);
    m_valuesPerBinSpin->setToolTip("Grey levels per bin. Smaller means a finer histogram.");

    m_thresholdSpin = new QSpinBox(this);
    m_thresholdSpin->setRange(0, kMaxValue16Bit);
    m_thresholdSpin->setValue(kMaxValue16Bit);
    m_thresholdSpin->setToolTip("Pixels at or above this value count as overexposed");

    m_overexposureLabel = new QLabel(this);

    QHBoxLayout *controls = new QHBoxLayout;
    controls->setContentsMargins(0, 0, 0, 0);
    controls->addWidget(m_logAxisCheck);
    controls->addSpacing(16);
    controls->addWidget(new QLabel("Values per bin", this));
    controls->addWidget(m_valuesPerBinSpin);
    controls->addSpacing(16);
    controls->addWidget(new QLabel("Overexposed >=", this));
    controls->addWidget(m_thresholdSpin);
    controls->addWidget(m_overexposureLabel, 1);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_histogram, 1);
    layout->addLayout(controls);

    connect(m_logAxisCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_histogram->setLogScale(checked);
    });

    connect(m_valuesPerBinSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                m_valuesPerBin = value;
                recompute();
            });

    connect(m_thresholdSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                m_threshold = value;
                recompute();
            });

    updateOverexposureReadout();
}

void HistogramWindow::setImage(const QImage &image)
{
    m_image = image;

    // 阈值控件的量程跟着图像格式走：8 位图配一个 65535 的默认阈值的话，
    // 永远没有像素能命中，过曝数会一直是 0，看着像没检测
    const int maxValue = (image.format() == QImage::Format_Grayscale16) ? kMaxValue16Bit : 255;
    if (maxValue != m_maxValue) {
        m_maxValue = maxValue;
        m_thresholdSpin->setRange(0, maxValue);
        // 每箱取值数不该超过总取值数，否则不管调多大都只有一个箱，
        // 那还不如把框直接限在值域大小上
        m_valuesPerBinSpin->setMaximum(maxValue + 1);
        // setRange() 会把超出去的值夹回来，这里把实际生效的阈值也收成夹后的值，
        // 免得 spinBox 上显示的和真正参与判定的是两个数
        m_threshold = m_thresholdSpin->value();
    }

    recompute();
}

void HistogramWindow::setOverexposureThreshold(int value)
{
    m_thresholdSpin->setValue(value);
}

bool HistogramWindow::isLogAxis() const
{
    return m_logAxisCheck->isChecked();
}

void HistogramWindow::setValuesPerBin(int value)
{
    m_valuesPerBinSpin->setValue(value);
}

// 一趟扫描同时出直方图和过曝数：live 模式下每帧都要跑，
// 而过曝判据就是那一个阈值，不值得为它再扫一遍
void HistogramWindow::recompute()
{
    m_binCenters.clear();
    m_binCounts.clear();
    m_overexposedCount = 0;
    m_totalPixels = 0;

    if (m_image.isNull()) {
        m_histogram->clearHistogram();
        updateOverexposureReadout();
        return;
    }

    const int width = m_image.width();
    const int height = m_image.height();
    m_totalPixels = width * height;

    // 值域是 0..maxValue 含两端，一共 maxValue+1 个取值。箱数由「每箱取值数」
    // 反推，再把箱宽摊回来，好让所有箱子正好铺满值域：否则最后一个箱子
    // 往往只剩几个取值，横坐标的量程也还会超出数据范围
    const int valueCount = m_maxValue + 1;
    const int binCount = qMax(1, (valueCount + m_valuesPerBin - 1) / m_valuesPerBin);
    const double binWidth = static_cast<double>(valueCount) / binCount;

    QVector<int> counts(binCount, 0);

    if (m_image.format() == QImage::Format_Grayscale16) {
        for (int y = 0; y < height; ++y) {
            const quint16 *line = reinterpret_cast<const quint16 *>(
                m_image.constBits() + y * m_image.bytesPerLine());
            for (int x = 0; x < width; ++x) {
                const int value = line[x];
                const int bin = qBound(0, static_cast<int>(value / binWidth), binCount - 1);
                ++counts[bin];
                if (value >= m_threshold) {
                    ++m_overexposedCount;
                }
            }
        }
    } else {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int value = qGray(m_image.pixel(x, y));
                const int bin = qBound(0, static_cast<int>(value / binWidth), binCount - 1);
                ++counts[bin];
                if (value >= m_threshold) {
                    ++m_overexposedCount;
                }
            }
        }
    }

    m_binCenters.reserve(binCount);
    m_binCounts.reserve(binCount);
    for (int i = 0; i < binCount; ++i) {
        m_binCenters.append((i + 0.5) * binWidth);
        m_binCounts.append(counts.at(i));
    }

    m_histogram->setHistogram(m_binCenters, m_binCounts);
    updateOverexposureReadout();
}

void HistogramWindow::updateOverexposureReadout()
{
    if (m_totalPixels <= 0) {
        m_overexposureLabel->setText(QStringLiteral("—"));
        return;
    }

    const double percent = 100.0 * m_overexposedCount / m_totalPixels;
    m_overexposureLabel->setText(QStringLiteral("%1 / %2  (%3%)")
                                      .arg(m_overexposedCount)
                                      .arg(m_totalPixels)
                                      .arg(percent, 0, 'f', 4));
}
