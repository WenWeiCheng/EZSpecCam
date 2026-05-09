#include "ProfileWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

#include "SpectrumViewWidget.h"

ProfileWindow::ProfileWindow(QWidget *parent)
    : QDialog(parent)
    , m_horizontalProfile(nullptr)
    , m_verticalProfile(nullptr)
    , m_horizontalLabel(nullptr)
    , m_verticalLabel(nullptr)
    , m_imageWidth(0)
    , m_imageHeight(0)
{
    setWindowTitle("Profile");
    setMinimumSize(800, 400);
    setModal(false);
    setupUi();
}

ProfileWindow::~ProfileWindow()
{
}

void ProfileWindow::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);

    QVBoxLayout *horizontalLayout = new QVBoxLayout();
    m_horizontalLabel = new QLabel("Horizontal Profile (Row Y)", this);
    m_horizontalProfile = new SpectrumViewWidget(this);
    horizontalLayout->addWidget(m_horizontalLabel);
    horizontalLayout->addWidget(m_horizontalProfile);

    QVBoxLayout *verticalLayout = new QVBoxLayout();
    m_verticalLabel = new QLabel("Vertical Profile (Column X)", this);
    m_verticalProfile = new SpectrumViewWidget(this);
    verticalLayout->addWidget(m_verticalLabel);
    verticalLayout->addWidget(m_verticalProfile);

    mainLayout->addLayout(horizontalLayout, 1);
    mainLayout->addLayout(verticalLayout, 1);
}

void ProfileWindow::updateProfile(int x, int y,
                                   const QVector<double> &horizontalData,
                                   const QVector<double> &verticalData)
{
    QVector<double> horizontalX(horizontalData.size());
    for (int i = 0; i < horizontalData.size(); ++i) {
        horizontalX[i] = i;
    }
    m_horizontalProfile->setData(horizontalX, horizontalData);

    QVector<double> verticalX(verticalData.size());
    for (int i = 0; i < verticalData.size(); ++i) {
        verticalX[i] = i;
    }
    m_verticalProfile->setData(verticalX, verticalData);

    m_horizontalLabel->setText(QString("Horizontal Profile (Row Y=%1)").arg(y));
    m_verticalLabel->setText(QString("Vertical Profile (Column X=%1)").arg(x));
}

void ProfileWindow::setImageSize(int width, int height)
{
    m_imageWidth = width;
    m_imageHeight = height;
}