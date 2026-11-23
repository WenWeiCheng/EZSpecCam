#include "StatisticsDialog.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

StatisticsDialog::StatisticsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Image Statistics"));
    setMinimumWidth(350);
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);
    auto *grid = new QGridLayout();

    auto *titleLabel = new QLabel(QStringLiteral("<h3>Statistics</h3>"), this);
    grid->addWidget(titleLabel, 0, 0, 1, 2);

    grid->addWidget(new QLabel(QStringLiteral("Data Type:"), this), 1, 0);
    m_dataTypeLabel = new QLabel(QStringLiteral("None"), this);
    grid->addWidget(m_dataTypeLabel, 1, 1);

    grid->addWidget(new QLabel(QStringLiteral("Pixel Count:"), this), 2, 0);
    m_pixelCountLabel = new QLabel(QStringLiteral("0"), this);
    grid->addWidget(m_pixelCountLabel, 2, 1);

    grid->addWidget(new QLabel(QStringLiteral("Minimum:"), this), 3, 0);
    m_minLabel = new QLabel(QStringLiteral("0"), this);
    grid->addWidget(m_minLabel, 3, 1);

    grid->addWidget(new QLabel(QStringLiteral("Maximum:"), this), 4, 0);
    m_maxLabel = new QLabel(QStringLiteral("0"), this);
    grid->addWidget(m_maxLabel, 4, 1);

    grid->addWidget(new QLabel(QStringLiteral("Average:"), this), 5, 0);
    m_avgLabel = new QLabel(QStringLiteral("0"), this);
    grid->addWidget(m_avgLabel, 5, 1);

    grid->addWidget(new QLabel(QStringLiteral("Std Dev:"), this), 6, 0);
    m_stdDevLabel = new QLabel(QStringLiteral("0"), this);
    grid->addWidget(m_stdDevLabel, 6, 1);

    mainLayout->addLayout(grid);

    m_closeButton = new QPushButton(QStringLiteral("Close"), this);
    m_closeButton->setDefault(true);
    connect(m_closeButton, &QPushButton::clicked, this, &StatisticsDialog::accept);
    mainLayout->addWidget(m_closeButton, 0, Qt::AlignRight);
}

StatisticsDialog::~StatisticsDialog() = default;

void StatisticsDialog::setImageData(const QImage &image)
{
    m_statistics = ImageStatistics::computeImage(image);
    updateDisplay();
}

void StatisticsDialog::setSpectrumData(const QVector<double> &data)
{
    m_statistics = ImageStatistics::computeSpectrum(data);
    updateDisplay();
}

void StatisticsDialog::updateDisplay()
{
    m_dataTypeLabel->setText(m_statistics.dataType);
    m_minLabel->setText(QString::number(m_statistics.minimum, 'f', 2));
    m_maxLabel->setText(QString::number(m_statistics.maximum, 'f', 2));
    m_avgLabel->setText(QString::number(m_statistics.average, 'f', 2));
    m_stdDevLabel->setText(QString::number(m_statistics.stdDeviation, 'f', 2));
    m_pixelCountLabel->setText(QString::number(m_statistics.pixelCount));
}
