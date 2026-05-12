#include "CameraTab.h"
#include "../../DebugMacros.h"
#include "ParameterWidgetFactory.h"
#include <QDebug>
#include <QObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTimer>
#include <QMetaObject>

CameraTab::CameraTab(QWidget *parent)
    : QWidget(parent)
    , connectButton(nullptr)
    , disconnectButton(nullptr)
    , cameraComboBox(nullptr)
    , captureModeComboBox(nullptr)
    , captureCountSpinBox(nullptr)
    , formLayout(nullptr)
    , m_countRow(-1)
    , parameterGroup(nullptr)
    , m_appController(nullptr)
    , m_statusLabel(nullptr)
    , m_dynamicParametersLayout(nullptr)
    , m_coolingTimer(new QTimer(this))
{
    setupUi();
    connect(m_coolingTimer, &QTimer::timeout, this, &CameraTab::onCoolingTimerTimeout);
}

CameraTab::~CameraTab()
{
}

void CameraTab::setAppController(AppController *controller)
{
    m_appController = controller;

    if (controller) {
        QStringList cameras = controller->availableCameras();
        cameraComboBox->clear();
        cameraComboBox->addItems(cameras);

        if (controller->isConnected()) {
            QString currentId = controller->currentCameraId();
            int index = cameraComboBox->findText(currentId);
            if (index >= 0) {
                cameraComboBox->setCurrentIndex(index);
            }
        }

        connect(controller, &AppController::stateChanged,
                this, &CameraTab::onCameraStateChanged);
        connect(controller, &AppController::connectCameraFinished,
                this, &CameraTab::onConnectCameraFinished);
        connect(controller, &AppController::disconnectCameraFinished,
                this, &CameraTab::onDisconnectCameraFinished);

        if (controller->isConnected()) {
            setBufferedConfig(controller->allParameters());
            updateConnectionState();
            buildDynamicParameterPanel();
            m_coolingTimer->start(100);
        }
    }

    updateConnectionState();
}

void CameraTab::updateConnectionState()
{
    if (!m_appController) {
        connectButton->setEnabled(false);
        disconnectButton->setEnabled(false);
        return;
    }

    CameraState state = m_appController->state();
    bool camera_notConnected = (state == CameraState::Disconnected || state == CameraState::Error);
    bool connected = !camera_notConnected && cameraComboBox->count() > 0;
    connectButton->setEnabled(!connected);
    disconnectButton->setEnabled(connected);
}

void CameraTab::onConnectButtonClicked()
{
    if (!m_appController || cameraComboBox->count() == 0) {
        return;
    }

    QString selectedCamera = cameraComboBox->currentText();
    if (!selectedCamera.isEmpty()) {
        connectButton->setEnabled(false);
        cameraComboBox->setEnabled(false);
        if (m_loadingIndicator) {
            m_loadingIndicator->setVisible(true);
            m_loadingIndicator->startAnimation();
        }
        m_statusLabel->setText("Connecting...");
        m_statusLabel->setVisible(true);

        QMetaObject::invokeMethod(m_appController, "connectCamera",
                                 Qt::QueuedConnection,
                                 Q_ARG(QString, selectedCamera));
    }
}

void CameraTab::onConnectCameraFinished(const QString &cameraId, bool success, const QString &error)
{
    Q_UNUSED(cameraId);

    if (m_loadingIndicator) {
        m_loadingIndicator->stopAnimation();
        m_loadingIndicator->setVisible(false);
    }
    cameraComboBox->setEnabled(true);

    if (success) {
        m_statusLabel->setText("Connected");
        m_statusLabel->setVisible(true);
        QTimer::singleShot(2000, this, [this]() {
            m_statusLabel->setVisible(false);
        });
    } else {
        m_statusLabel->setText(error.isEmpty() ? "Connection failed" : error);
        m_statusLabel->setVisible(true);
        connectButton->setEnabled(true);
    }

    updateConnectionState();
}

void CameraTab::onDisconnectCameraFinished(const QString &cameraId)
{
    Q_UNUSED(cameraId);
    updateConnectionState();
}

void CameraTab::onDisconnectButtonClicked()
{
    if (!m_appController) {
        return;
    }

    QMetaObject::invokeMethod(m_appController, "disconnectCamera",
                             Qt::QueuedConnection);
}

void CameraTab::onCaptureModeChanged(int index)
{
    Q_UNUSED(index);
    if (captureModeComboBox && formLayout && m_countRow >= 0) {
        bool isBurst = (captureModeComboBox->currentText() == "Burst");
        formLayout->setRowVisible(m_countRow, isBurst);
    }
}

void CameraTab::applyCaptureMode()
{
    if (!m_appController || !captureModeComboBox) {
        return;
    }

    int count = 1;
    QString mode = captureModeComboBox->currentText();
    if (mode == "Live") {
        count = 0;
    } else if (mode == "Burst") {
        count = captureCountSpinBox ? captureCountSpinBox->value() : 10;
    }

    QVariantMap params;
    params["captureCount"] = count;
    m_appController->setParameters(params);
}

int CameraTab::getCaptureCount() const
{
    if (!captureModeComboBox) {
        return 0;
    }
    QString mode = captureModeComboBox->currentText();
    if (mode == QStringLiteral("Live")) {
        return 0;
    }
    if (mode == QStringLiteral("Single")) {
        return 1;
    }
    return captureCountSpinBox ? captureCountSpinBox->value() : 10;
}

void CameraTab::onCameraSelected(int index)
{
    Q_UNUSED(index);
    updateConnectionState();
}

void CameraTab::onCameraStateChanged(CameraState state)
{
    if (state == CameraState::Connected) {
        if (m_parameterWidgets.isEmpty()) {
            setBufferedConfig(m_appController->allParameters());
            buildDynamicParameterPanel();
            m_coolingTimer->start(100);
            if (parameterGroup) {
                parameterGroup->setVisible(true);
            }
        }
    } else if (state == CameraState::Disconnected || state == CameraState::Error) {
        clearDynamicParameterPanel();
        m_coolingTimer->stop();
        if (parameterGroup) {
            parameterGroup->setVisible(false);
        }
    }
    updateConnectionState();
}

// TODO: UI 放在专门的文件里组织
void CameraTab::setupUi()
{
    formLayout = new QFormLayout(this);
    formLayout->setSpacing(6);
    formLayout->setLabelAlignment(Qt::AlignRight);

    cameraComboBox = new QComboBox(this);
    cameraComboBox->setObjectName("cameraComboBox");
    cameraComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    formLayout->addRow("Camera:", cameraComboBox);

    connectButton = new QPushButton("Connect", this);
    connectButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    disconnectButton = new QPushButton("Disconnect", this);
    disconnectButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    disconnectButton->setEnabled(false);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setVisible(false);

    m_loadingIndicator = new LoadingIndicator(this);
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

    connect(connectButton, &QPushButton::clicked,
            this, &CameraTab::onConnectButtonClicked);
    connect(disconnectButton, &QPushButton::clicked,
            this, &CameraTab::onDisconnectButtonClicked);
    connect(cameraComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraTab::onCameraSelected);

    captureModeComboBox = new QComboBox(this);
    captureModeComboBox->addItems({"Single", "Burst", "Live"});
    captureModeComboBox->setCurrentText("Single");
    captureModeComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    captureCountSpinBox = new QSpinBox(this);
    captureCountSpinBox->setRange(2, 1000);
    captureCountSpinBox->setValue(5);
    captureCountSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    formLayout->addRow("Capture Mode:", captureModeComboBox);
    m_countRow = formLayout->rowCount();
    formLayout->addRow("Count:", captureCountSpinBox);
    formLayout->setRowVisible(m_countRow, false);

    connect(captureModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraTab::onCaptureModeChanged);

    parameterGroup = new QGroupBox("Parameters", this);
    m_dynamicParametersLayout = new QVBoxLayout();
    parameterGroup->setLayout(m_dynamicParametersLayout);
    parameterGroup->setVisible(false);

    QWidget *scrollContent = new QWidget(this);
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->addWidget(parameterGroup);
    scrollLayout->addStretch();

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(scrollContent);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    formLayout->addRow(m_scrollArea);
}

void CameraTab::buildDynamicParameterPanel()
{
    if (!m_appController || !m_appController->isConnected()) {
        return;
    }

    clearDynamicParameterPanel();

    ICameraDriver *driver = m_appController->driver();
    if (!driver) {
        return;
    }

    QStringList paramNames = driver->parameterNames();

    QMap<ParameterCategory, QVector<QPair<QString, ParameterDefinition>>> paramsByCategory;

    for (const QString &paramName : paramNames) {
        ParameterDefinition def = driver->parameter(paramName);
        m_parameterDefinitions.insert(paramName, def);
        paramsByCategory[def.category].append({paramName, def});
    }

    static const QMap<ParameterCategory, QString> categoryNames = {
        {ParameterCategory::Core, "Core"},
        {ParameterCategory::Cooling, "Cooling"},
        {ParameterCategory::Info, "Info"},
        {ParameterCategory::Advanced, "Advanced"},
        {ParameterCategory::Debug, "Debug"}
    };

    for (auto it = paramsByCategory.constBegin(); it != paramsByCategory.constEnd(); ++it) {
        ParameterCategory category = it.key();
        QVector<QPair<QString, ParameterDefinition>> sortedParams = it.value();

        std::sort(sortedParams.begin(), sortedParams.end(),
            [](const auto &a, const auto &b) {
                return a.second.order < b.second.order;
            });

        if (sortedParams.isEmpty()) {
            continue;
        }

        QString groupName = categoryNames.value(category, "Other");
        QGroupBox *categoryGroup = new QGroupBox(groupName, this);
        QFormLayout *paramFormLayout = new QFormLayout();

        for (const auto &paramPair : sortedParams) {
            const QString &name = paramPair.first;
            const ParameterDefinition &def = paramPair.second;

            QWidget *widget = ParameterWidgetFactory::createWidget(def);

            if (widget) {
                m_parameterWidgets.insert(name, widget);
                if (m_bufferedConfig.contains(name)) {
                    ParameterWidgetFactory::setWidgetValue(widget, m_bufferedConfig.value(name), def.type);
                    CONFIG_DEBUG << "Set initial value for" << name << ":" << m_bufferedConfig.value(name);
                }
                paramFormLayout->addRow(def.displayName + ":", widget);
            }
        }

        categoryGroup->setLayout(paramFormLayout);
        m_categoryGroups.insert(category, categoryGroup);
        m_dynamicParametersLayout->addWidget(categoryGroup);
    }

    m_statusLabel->setVisible(false);
}

void CameraTab::clearDynamicParameterPanel()
{
    m_statusLabel->setVisible(false);

    while (QLayoutItem *item = m_dynamicParametersLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    for (QGroupBox *group : m_categoryGroups.values()) {
        if (group) {
            group->deleteLater();
        }
    }
    m_categoryGroups.clear();

    for (QWidget *widget : m_parameterWidgets.values()) {
        if (widget) {
            widget->deleteLater();
        }
    }
    m_parameterWidgets.clear();
    m_parameterDefinitions.clear();
}

void CameraTab::refreshCameraList()
{
    if (!m_appController) {
        return;
    }
    QStringList cameras = m_appController->availableCameras();
    cameraComboBox->clear();
    cameraComboBox->addItems(cameras);

    if (m_appController->isConnected()) {
        QString currentId = m_appController->currentCameraId();
        int index = cameraComboBox->findText(currentId);
        if (index >= 0) {
            cameraComboBox->setCurrentIndex(index);
        }
    }

    updateConnectionState();
}

void CameraTab::setBufferedConfig(const QVariantMap &config)
{
    m_bufferedConfig = config;
}

void CameraTab::updateBufferedConfigFromWidgets()
{
    if (captureModeComboBox) {
        m_bufferedConfig["captureMode"] = captureModeComboBox->currentText();
    }
    if (captureCountSpinBox) {
        m_bufferedConfig["captureCount"] = captureCountSpinBox->value();
        QString captureMode = captureModeComboBox->currentText();
        if (captureMode == "Single") {
            m_bufferedConfig["captureCount"] = 1;
        } else if (captureMode == "Live") {
            m_bufferedConfig["captureCount"] = 0;
        }
    }

    for (auto it = m_parameterWidgets.constBegin(); it != m_parameterWidgets.constEnd(); ++it) {
        const QString &paramName = it.key();
        QWidget *widget = it.value();

        auto defIt = m_parameterDefinitions.constFind(paramName);
        if (defIt == m_parameterDefinitions.constEnd()) {
            continue;
        }
        if (defIt.value().isReadOnly) {
            continue;
        }

        QVariant value = ParameterWidgetFactory::getWidgetValue(widget, defIt.value().type);
        if (value.isValid()) {
            m_bufferedConfig.insert(paramName, value);
        } else {
            qWarning() << "Invalid value for parameter" << paramName;
        }
    }
}

QVariantMap CameraTab::getBufferedConfig() const
{
    return m_bufferedConfig;
}

void CameraTab::onCoolingTimerTimeout()
{
    if (!m_appController || !m_appController->isConnected()) {
        return;
    }

    ICameraDriver *driver = m_appController->driver();
    if (!driver) {
        return;
    }

    QStringList paramNames = driver->parameterNames();
    for (const QString &paramName : paramNames) {
        ParameterDefinition def = driver->parameter(paramName);
        if (def.isDynamic && def.isExtrinsic && def.category == ParameterCategory::Cooling) {
            QVariant value = driver->parameterValue(paramName);
            QWidget *widget = m_parameterWidgets.value(paramName);
            if (widget) {
                if (def.type == ParameterType::String) {
                    QLabel *label = qobject_cast<QLabel*>(widget);
                    if (label) {
                        label->setText(value.toString());
                    }
                } else if (def.type == ParameterType::FloatRange) {
                    QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox*>(widget);
                    if (spinBox) {
                        spinBox->setValue(value.toDouble());
                    }
                } else if (def.type == ParameterType::IntRange) {
                    QSpinBox *spinBox = qobject_cast<QSpinBox*>(widget);
                    if (spinBox) {
                        spinBox->setValue(value.toInt());
                    }
                }
            }
        }
    }
}

void CameraTab::onParametersCommitted()
{
    m_statusLabel->setVisible(false);

    if (!m_appController || !m_appController->isConnected()) {
        return;
    }

    ICameraDriver *driver = m_appController->driver();
    if (!driver) {
        return;
    }

    QStringList paramNames = driver->parameterNames();
    for (const QString &paramName : paramNames) {
        ParameterDefinition def = driver->parameter(paramName);
        if (def.isDynamic) {
            QVariant value = driver->parameterValue(paramName);
            QWidget *widget = m_parameterWidgets.value(paramName);
            if (widget) {
                if (def.type == ParameterType::IntRange) {
                    QSpinBox *spinBox = qobject_cast<QSpinBox*>(widget);
                    if (spinBox) {
                        spinBox->setValue(value.toInt());
                    }
                } else if (def.type == ParameterType::FloatRange) {
                    QDoubleSpinBox *spinBox = qobject_cast<QDoubleSpinBox*>(widget);
                    if (spinBox) {
                        spinBox->setValue(value.toDouble());
                    }
                }
            }
        }
    }
}