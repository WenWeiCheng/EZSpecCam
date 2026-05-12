#include "CameraTab.h"
#include "../../DebugMacros.h"
#include "../../ui/CameraTabUi.h"
#include "ParameterWidgetFactory.h"
#include <QDebug>
#include <QObject>
#include <QTimer>
#include <QMetaObject>
#include <QSettings>
#include <QComboBox>
#include <QPushButton>

CameraTab::CameraTab(QWidget *parent)
    : QWidget(parent)
    , ui(new CameraTabUi(this))
    , m_appController(nullptr)
    , m_coolingTimer(new QTimer(this))
{
    ui->setupUi(this);
    connect(m_coolingTimer, &QTimer::timeout, this, &CameraTab::onCoolingTimerTimeout);
    connect(ui->connectButton, &QPushButton::clicked,
            this, &CameraTab::onConnectButtonClicked);
    connect(ui->disconnectButton, &QPushButton::clicked,
            this, &CameraTab::onDisconnectButtonClicked);
    connect(ui->cameraComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraTab::onCameraSelected);
    connect(ui->captureModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraTab::onCaptureModeChanged);
}

CameraTab::~CameraTab()
{
}

void CameraTab::setAppController(AppController *controller)
{
    m_appController = controller;

    if (controller) {
        QStringList cameras = controller->availableCameras();
        ui->cameraComboBox->clear();
        ui->cameraComboBox->addItems(cameras);

        if (controller->isConnected()) {
            QString currentId = controller->currentCameraId();
            int index = ui->cameraComboBox->findText(currentId);
            if (index >= 0) {
                ui->cameraComboBox->setCurrentIndex(index);
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
        ui->connectButton->setEnabled(false);
        ui->disconnectButton->setEnabled(false);
        return;
    }

    CameraState state = m_appController->state();
    bool camera_notConnected = (state == CameraState::Disconnected || state == CameraState::Error);
    bool connected = !camera_notConnected && ui->cameraComboBox->count() > 0;
    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
}

void CameraTab::onConnectButtonClicked()
{
    if (!m_appController || ui->cameraComboBox->count() == 0) {
        return;
    }

    QString selectedCamera = ui->cameraComboBox->currentText();
    if (!selectedCamera.isEmpty()) {
        ui->connectButton->setEnabled(false);
        ui->cameraComboBox->setEnabled(false);
        if (ui->m_loadingIndicator) {
            ui->m_loadingIndicator->setVisible(true);
            ui->m_loadingIndicator->startAnimation();
        }
        ui->m_statusLabel->setText("Connecting...");
        ui->m_statusLabel->setVisible(true);

        QMetaObject::invokeMethod(m_appController, "connectCamera",
                                 Qt::QueuedConnection,
                                 Q_ARG(QString, selectedCamera));
    }
}

void CameraTab::onConnectCameraFinished(const QString &cameraId, bool success, const QString &error)
{
    Q_UNUSED(cameraId);

    if (ui->m_loadingIndicator) {
        ui->m_loadingIndicator->stopAnimation();
        ui->m_loadingIndicator->setVisible(false);
    }
    ui->cameraComboBox->setEnabled(true);

    if (success) {
        ui->m_statusLabel->setText("Connected");
        ui->m_statusLabel->setVisible(true);
        QTimer::singleShot(2000, this, [this]() {
            ui->m_statusLabel->setVisible(false);
        });
    } else {
        ui->m_statusLabel->setText(error.isEmpty() ? "Connection failed" : error);
        ui->m_statusLabel->setVisible(true);
        ui->connectButton->setEnabled(true);
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
    if (ui->captureModeComboBox && ui->formLayout && ui->m_countRow >= 0) {
        bool isBurst = (ui->captureModeComboBox->currentText() == "Burst");
        ui->formLayout->setRowVisible(ui->m_countRow, isBurst);
    }
}

void CameraTab::applyCaptureMode()
{
    if (!m_appController || !ui->captureModeComboBox) {
        return;
    }

    int count = 1;
    QString mode = ui->captureModeComboBox->currentText();
    if (mode == "Live") {
        count = 0;
    } else if (mode == "Burst") {
        count = ui->captureCountSpinBox ? ui->captureCountSpinBox->value() : 10;
    }

    QVariantMap params;
    params["captureCount"] = count;
    m_appController->setParameters(params);
}

int CameraTab::getCaptureCount() const
{
    if (!ui->captureModeComboBox) {
        return 0;
    }
    QString mode = ui->captureModeComboBox->currentText();
    if (mode == QStringLiteral("Live")) {
        return 0;
    }
    if (mode == QStringLiteral("Single")) {
        return 1;
    }
    return ui->captureCountSpinBox ? ui->captureCountSpinBox->value() : 10;
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
            if (ui->parameterGroup) {
                ui->parameterGroup->setVisible(true);
            }
        }
    } else if (state == CameraState::Disconnected || state == CameraState::Error) {
        clearDynamicParameterPanel();
        m_coolingTimer->stop();
        if (ui->parameterGroup) {
            ui->parameterGroup->setVisible(false);
        }
    }
    updateConnectionState();
}

void CameraTab::buildDynamicParameterPanel()
{
    if (!m_appController || !m_appController->isConnected()) {
        return;
    }

    clearDynamicParameterPanel();

    QStringList paramNames = m_appController->parameterNames();

    QMap<ParameterCategory, QVector<QPair<QString, ParameterDefinition>>> paramsByCategory;

    for (const QString &paramName : paramNames) {
        ParameterDefinition def = m_appController->parameter(paramName);
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
        ui->m_dynamicParametersLayout->addWidget(categoryGroup);
    }

    ui->m_statusLabel->setVisible(false);
}

void CameraTab::clearDynamicParameterPanel()
{
    ui->m_statusLabel->setVisible(false);

    while (QLayoutItem *item = ui->m_dynamicParametersLayout->takeAt(0)) {
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
    ui->cameraComboBox->clear();
    ui->cameraComboBox->addItems(cameras);

    if (m_appController->isConnected()) {
        QString currentId = m_appController->currentCameraId();
        int index = ui->cameraComboBox->findText(currentId);
        if (index >= 0) {
            ui->cameraComboBox->setCurrentIndex(index);
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
    if (ui->captureModeComboBox) {
        m_bufferedConfig["captureMode"] = ui->captureModeComboBox->currentText();
    }
    if (ui->captureCountSpinBox) {
        m_bufferedConfig["captureCount"] = ui->captureCountSpinBox->value();
        QString captureMode = ui->captureModeComboBox->currentText();
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
            CONFIG_DEBUG << "Invalid value for" << paramName << ":" << value;
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

    QStringList paramNames = m_appController->parameterNames();
    for (const QString &paramName : paramNames) {
        ParameterDefinition def = m_appController->parameter(paramName);
        if (def.isDynamic && def.isExtrinsic && def.category == ParameterCategory::Cooling) {
            QVariant value = m_appController->parameterValue(paramName);
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
    ui->m_statusLabel->setVisible(false);

    if (!m_appController || !m_appController->isConnected()) {
        return;
    }

    QStringList paramNames = m_appController->parameterNames();
    for (const QString &paramName : paramNames) {
        ParameterDefinition def = m_appController->parameter(paramName);
        if (def.isDynamic) {
            QVariant value = m_appController->parameterValue(paramName);
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