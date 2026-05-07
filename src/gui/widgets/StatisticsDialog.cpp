#include "StatisticsDialog.h"

#include <QGridLayout>
#include <QLabel>
#include <QDebug>
#include <cmath>
#include <limits>

StatisticsDialog::StatisticsDialog(QWidget *parent)
    : QDialog(parent)
    , m_min(0)
    , m_max(0)
    , m_avg(0)
    , m_stdDev(0)
    , m_pixelCount(0)
{
    setWindowTitle("Image Statistics");
    setMinimumWidth(350);
    setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QGridLayout *grid = new QGridLayout();

    QLabel *titleLabel = new QLabel("<h3>Statistics</h3>", this);
    grid->addWidget(titleLabel, 0, 0, 1, 2);

    grid->addWidget(new QLabel("Data Type:", this), 1, 0);
    m_dataTypeLabel = new QLabel("None", this);
    grid->addWidget(m_dataTypeLabel, 1, 1);

    grid->addWidget(new QLabel("Pixel Count:", this), 2, 0);
    m_pixelCountLabel = new QLabel("0", this);
    grid->addWidget(m_pixelCountLabel, 2, 1);

    grid->addWidget(new QLabel("Minimum:", this), 3, 0);
    m_minLabel = new QLabel("0", this);
    grid->addWidget(m_minLabel, 3, 1);

    grid->addWidget(new QLabel("Maximum:", this), 4, 0);
    m_maxLabel = new QLabel("0", this);
    grid->addWidget(m_maxLabel, 4, 1);

    grid->addWidget(new QLabel("Average:", this), 5, 0);
    m_avgLabel = new QLabel("0", this);
    grid->addWidget(m_avgLabel, 5, 1);

    grid->addWidget(new QLabel("Std Dev:", this), 6, 0);
    m_stdDevLabel = new QLabel("0", this);
    grid->addWidget(m_stdDevLabel, 6, 1);

    mainLayout->addLayout(grid);

    m_closeButton = new QPushButton("Close", this);
    m_closeButton->setDefault(true);
    connect(m_closeButton, &QPushButton::clicked, this, &StatisticsDialog::accept);
    mainLayout->addWidget(m_closeButton, 0, Qt::AlignRight);
}

StatisticsDialog::~StatisticsDialog()
{
}

void StatisticsDialog::setImageData(const QImage &image)
{
    calculateImageStatistics(image);
    updateDisplay();
}

void StatisticsDialog::setSpectrumData(const QVector<double> &data)
{
    calculateSpectrumStatistics(data);
    updateDisplay();
}

void StatisticsDialog::calculateImageStatistics(const QImage &image)
{
    if (image.isNull()) {
        m_min = 0;
        m_max = 0;
        m_avg = 0;
        m_stdDev = 0;
        m_pixelCount = 0;
        m_dataType = "None";
        return;
    }

    const uchar *bits = image.constBits();
    int width = image.width();
    int height = image.height();
    int bytesPerLine = image.bytesPerLine();

    double sum = 0;
    double sumSq = 0;
    quint64 count = 0;

    m_min = std::numeric_limits<double>::max();
    m_max = std::numeric_limits<double>::min();

    QImage::Format format = image.format();

    for (int y = 0; y < height; ++y) {
        const uchar *row = bits + y * bytesPerLine;
        for (int x = 0; x < width; ++x) {
            double val = 0;

            if (format == QImage::Format_Grayscale8 ||
                format == QImage::Format_Indexed8) {
                val = row[x];
            }
            else if (format == QImage::Format_Grayscale16) {
                const ushort *row16 = reinterpret_cast<const ushort*>(row);
                val = row16[x];
            }
            else if (format == QImage::Format_RGB16) {
                const ushort *row16 = reinterpret_cast<const ushort*>(row);
                val = row16[x];
            }
            else if (format == QImage::Format_RGB888) {
                val = 0.299 * row[x*3] + 0.587 * row[x*3+1] + 0.114 * row[x*3+2];
            }
            else if (format == QImage::Format_RGB32 ||
                     format == QImage::Format_ARGB32 ||
                     format == QImage::Format_RGBA64) {
                const QRgb *pixel = reinterpret_cast<const QRgb*>(row + x * 4);
                val = 0.299 * qRed(*pixel) + 0.587 * qGreen(*pixel) + 0.114 * qBlue(*pixel);
            }
            else {
                val = row[x];
            }

            if (val < m_min) m_min = val;
            if (val > m_max) m_max = val;
            sum += val;
            sumSq += val * val;
            ++count;
        }
    }

    m_pixelCount = count;
    m_avg = count > 0 ? sum / count : 0;
    m_stdDev = count > 0 ? std::sqrt(sumSq / count - m_avg * m_avg) : 0;
    m_dataType = (format == QImage::Format_RGB16 ||
                  format == QImage::Format_RGBA64 ||
                  format == QImage::Format_Grayscale16) ? "16-bit" : "8-bit";
}

void StatisticsDialog::calculateSpectrumStatistics(const QVector<double> &data)
{
    if (data.isEmpty()) {
        m_min = 0;
        m_max = 0;
        m_avg = 0;
        m_stdDev = 0;
        m_pixelCount = 0;
        m_dataType = "None";
        return;
    }

    double sum = 0;
    double sumSq = 0;

    m_min = std::numeric_limits<double>::max();
    m_max = std::numeric_limits<double>::min();

    for (double val : data) {
        if (val < m_min) m_min = val;
        if (val > m_max) m_max = val;
        sum += val;
        sumSq += val * val;
    }

    m_pixelCount = data.size();
    m_avg = sum / m_pixelCount;
    m_stdDev = std::sqrt(sumSq / m_pixelCount - m_avg * m_avg);
    m_dataType = "Spectrum";
}

void StatisticsDialog::updateDisplay()
{
    m_dataTypeLabel->setText(m_dataType);
    m_minLabel->setText(QString::number(m_min, 'f', 2));
    m_maxLabel->setText(QString::number(m_max, 'f', 2));
    m_avgLabel->setText(QString::number(m_avg, 'f', 2));
    m_stdDevLabel->setText(QString::number(m_stdDev, 'f', 2));
    m_pixelCountLabel->setText(QString::number(m_pixelCount));
}