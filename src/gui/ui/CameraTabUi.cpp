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
    formLayout->addRow("Camera:", cameraComboBox);

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
    buttonLayout->addStretch();
    formLayout->addRow("", buttonLayout);

    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_loadingIndicator);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    formLayout->addRow("", statusLayout);

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
    formLayout->setRowVisible(m_countRow, false);

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
