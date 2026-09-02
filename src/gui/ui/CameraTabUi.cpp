#include "CameraTabUi.h"
#include "../widgets/config/CameraTab.h"
#include "../widgets/config/LoadingIndicator.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QScrollArea>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QSizePolicy>

CameraTabUi::CameraTabUi(QObject *parent)
    : QObject(parent)
    , scanButton(nullptr)
    , connectButton(nullptr)
    , disconnectButton(nullptr)
    , cameraComboBox(nullptr)
    , captureModeComboBox(nullptr)
    , captureCountSpinBox(nullptr)
    , formLayout(nullptr)
    , m_countRow(-1)
    , parameterGroup(nullptr)
    , m_statusLabel(nullptr)
    , m_loadingIndicator(nullptr)
    , m_scanStatusLabel(nullptr)
    , m_dynamicParametersLayout(nullptr)
    , m_scrollArea(nullptr)
{
}

CameraTabUi::~CameraTabUi()
{
}

void CameraTabUi::setupUi(CameraTab *tab)
{
    formLayout = new QFormLayout(tab);
    formLayout->setSpacing(6);
    formLayout->setLabelAlignment(Qt::AlignRight);

    cameraComboBox = new QComboBox(tab);
    cameraComboBox->setObjectName("cameraComboBox");
    cameraComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    cameraComboBox->setEnabled(false);

    scanButton = new QPushButton("Scan", tab);
    scanButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    QHBoxLayout *cameraLayout = new QHBoxLayout();
    cameraLayout->addWidget(scanButton);
    cameraLayout->addWidget(cameraComboBox, 1);
    formLayout->addRow("Camera:", cameraLayout);

    connectButton = new QPushButton("Connect", tab);
    connectButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    disconnectButton = new QPushButton("Disconnect", tab);
    disconnectButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    disconnectButton->setEnabled(false);

    m_statusLabel = new QLabel(tab);
    m_statusLabel->setVisible(false);

    m_loadingIndicator = new LoadingIndicator(tab);
    m_loadingIndicator->setFixedSize(24, 24);
    m_loadingIndicator->setVisible(false);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(connectButton);
    buttonLayout->addWidget(disconnectButton);
    formLayout->addRow("", buttonLayout);

    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_loadingIndicator);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    formLayout->addRow("", statusLayout);

    m_scanStatusLabel = new QLabel(tab);
    m_scanStatusLabel->setVisible(false);
    formLayout->addRow("", m_scanStatusLabel);

    captureModeComboBox = new QComboBox(tab);
    captureModeComboBox->addItems({"Single", "Burst", "Live"});
    captureModeComboBox->setCurrentText("Single");
    captureModeComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    captureCountSpinBox = new QSpinBox(tab);
    captureCountSpinBox->setRange(2, 1000);
    captureCountSpinBox->setValue(5);
    captureCountSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    formLayout->addRow("Capture Mode:", captureModeComboBox);
    m_countRow = formLayout->rowCount();
    formLayout->addRow("Count:", captureCountSpinBox);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    formLayout->setRowVisible(m_countRow, false);
#endif

    parameterGroup = new QGroupBox("Parameters", tab);
    m_dynamicParametersLayout = new QVBoxLayout();
    parameterGroup->setLayout(m_dynamicParametersLayout);
    parameterGroup->setVisible(false);

    QWidget *scrollContent = new QWidget(tab);
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->addWidget(parameterGroup);
    scrollLayout->addStretch();

    m_scrollArea = new QScrollArea(tab);
    m_scrollArea->setWidget(scrollContent);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    formLayout->addRow(m_scrollArea);
}
