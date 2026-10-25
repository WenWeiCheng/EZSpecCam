#include "CameraTab.h"
#include "../../DebugMacros.h"
#include "../../Theme.h"
#include "../../ui/CameraTabUi.h"
#include "../display/ParameterHistoryWindow.h"
#include "ParameterHistoryStore.h"
#include "ParameterWidgetFactory.h"
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMetaObject>
#include <QObject>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QToolButton>

namespace {

// How often the read-only telemetry labels are refreshed. Each tick costs one
// blocking UART round trip per dynamic parameter (~9 ms on hk16011), paid on the
// GUI thread, so the interval is deliberately coarse: a 1 s refresh is plenty
// for temperatures and acquisition state, and it leaves the GUI thread to frame
// rendering.
constexpr int kTelemetryPollMs = 1000;

// 行内曲线按钮的图标。调色板里没有现成的折线图标，画一个：左下两条轴 +
// 一条折线，颜色取按钮文字色，主题切换时由 updatePlotButtonIcons() 重画
QIcon makePlotIcon(const QPalette &palette)
{
    // 16 px 显示尺寸的 2 倍，缩小后用在高 DPI 屏上也还清楚
    constexpr int kSize = 32;

    QPixmap pixmap(kSize, kSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(palette.color(QPalette::ButtonText), 2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);

    painter.drawLine(QPointF(5.0, 5.0), QPointF(5.0, 27.0));
    painter.drawLine(QPointF(5.0, 27.0), QPointF(28.0, 27.0));

    QPolygonF curve;
    curve << QPointF(9.0, 21.0)
          << QPointF(15.0, 13.0)
          << QPointF(20.0, 18.0)
          << QPointF(27.0, 8.0);
    painter.drawPolyline(curve);

    return QIcon(pixmap);
}

} // namespace

CameraTab::CameraTab(QWidget *parent)
    : QWidget(parent)
    , ui(new CameraTabUi(this))
    , m_appController(nullptr)
    , m_telemetryTimer(new QTimer(this))
    , m_history(new ParameterHistoryStore(this))
{
    ui->setupUi(this);
    connect(m_telemetryTimer, &QTimer::timeout, this, &CameraTab::onTelemetryTimerTimeout);
    connect(Theme::instance(), &Theme::themeChanged, this, &CameraTab::updatePlotButtonIcons);
    connect(ui->scanButton, &QPushButton::clicked,
            this, &CameraTab::onScanButtonClicked);
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
        ui->cameraComboBox->setEnabled(!cameras.isEmpty());

        if (controller->isActive()) {
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
        connect(controller, &AppController::pluginScanProgress,
                this, &CameraTab::onScanProgress);
        connect(controller, &AppController::pluginScanStarted,
                this, &CameraTab::onScanStarted);
        connect(controller, &AppController::pluginScanCompleted,
                this, &CameraTab::onScanCompleted);
        connect(controller, &AppController::pluginLoadFailed,
                this, [this](const QString &, const QString &) {
            m_lastScanFailed++;
        });
        connect(controller, &AppController::setParametersFinished,
                this, &CameraTab::onSetParametersFinished);
        connect(controller, &AppController::commitParametersFinished,
                this, &CameraTab::onCommitParametersFinished);

        if (controller->isActive()) {
            setBufferedConfig(controller->allParameters());
            m_committedConfig = controller->allParameters();
            updateConnectionState();
            buildDynamicParameterPanel();
            m_telemetryTimer->start(kTelemetryPollMs);
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

void CameraTab::onScanButtonClicked()
{
    if (!m_appController) {
        return;
    }

    m_lastScanFailed = 0;
    ui->cameraComboBox->setEnabled(false);
    if (ui->m_scanStatusLabel) {
        ui->m_scanStatusLabel->setText("Scanning plugins...");
        ui->m_scanStatusLabel->setVisible(true);
    }

    QMetaObject::invokeMethod(m_appController, &AppController::scanPlugins, Qt::QueuedConnection);
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
        // 新会话：历史从干净状态重新开始。断开时不清，窗口里还能继续看/导出
        m_history->clear();
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        ui->formLayout->setRowVisible(ui->m_countRow, isBurst);
#endif
    }
    emit captureModeChanged(getCaptureMode());
}

QString CameraTab::getCaptureMode() const
{
    if (!ui->captureModeComboBox) {
        return QStringLiteral("Single");
    }
    return ui->captureModeComboBox->currentText();
}

void CameraTab::setCaptureMode(const QString &mode)
{
    if (!ui->captureModeComboBox) {
        return;
    }
    const int index = ui->captureModeComboBox->findText(mode);
    if (index < 0) {
        return;
    }
    // 已经是这一项就别再动，setCurrentIndex 相同值虽不复发信号，但这里
    // 保持「改了什么就发什么」的语义更清楚
    if (ui->captureModeComboBox->currentIndex() == index) {
        return;
    }
    ui->captureModeComboBox->setCurrentIndex(index);
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
    const QString mode = getCaptureMode();
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
            m_telemetryTimer->start(kTelemetryPollMs);
            if (ui->parameterGroup) {
                ui->parameterGroup->setVisible(true);
            }
        }
    } else if (state == CameraState::Disconnected || state == CameraState::Error) {
        clearDynamicParameterPanel();
        m_telemetryTimer->stop();
        if (ui->parameterGroup) {
            ui->parameterGroup->setVisible(false);
        }
    }
    updateConnectionState();
}

void CameraTab::buildDynamicParameterPanel()
{
    if (!m_appController || !m_appController->isActive()) {
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

            const ParameterField field = createParameterField(name, def);

            if (field.widget) {
                m_parameterWidgets.insert(name, field.widget);
                registerParameterField(name, field);
                if (m_bufferedConfig.contains(name)) {
                    ParameterWidgetFactory::setWidgetValue(field.widget, m_bufferedConfig.value(name), def);
                    CONFIG_DEBUG << "Set widget value for" << name << ":" << m_bufferedConfig.value(name);
                }
                paramFormLayout->addRow(def.displayName + ":", field.widget);
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
    m_telemetryLabels.clear();
    m_plotButtons.clear();
}

void CameraTab::rebuildParameterWidget(const QString &paramName)
{
    if (!m_appController) {
        return;
    }

    ParameterDefinition newDef = m_appController->parameter(paramName);
    QGroupBox *categoryGroup = m_categoryGroups.value(newDef.category);
    if (!categoryGroup) {
        return;
    }

    QFormLayout *formLayout = qobject_cast<QFormLayout*>(categoryGroup->layout());
    if (!formLayout) {
        return;
    }

    QWidget *oldWidget = m_parameterWidgets.value(paramName);
    if (!oldWidget) {
        return;
    }

    int row = formLayout->rowCount();
    for (int i = 0; i < formLayout->rowCount(); ++i) {
        if (formLayout->itemAt(i, QFormLayout::FieldRole)->widget() == oldWidget) {
            row = i;
            break;
        }
    }
    if (row == formLayout->rowCount()) {
        return;
    }

    const ParameterField field = createParameterField(paramName, newDef);
    if (!field.widget) {
        return;
    }

    if (m_bufferedConfig.contains(paramName)) {
        ParameterWidgetFactory::setWidgetValue(field.widget, m_bufferedConfig.value(paramName), newDef);
    }

    formLayout->replaceWidget(oldWidget, field.widget);
    unregisterParameterField(paramName);
    oldWidget->deleteLater();

    m_parameterWidgets.insert(paramName, field.widget);
    registerParameterField(paramName, field);
    m_parameterDefinitions.insert(paramName, newDef);
}

CameraTab::ParameterField CameraTab::createParameterField(const QString &paramName,
                                                          const ParameterDefinition &def)
{
    ParameterField field;
    field.widget = ParameterWidgetFactory::createWidget(def);
    if (!field.widget) {
        return field;
    }

    if (ParameterHistoryStore::isTelemetryParameter(def)) {
        field.label = qobject_cast<QLabel *>(field.widget);
    }

    if (!ParameterHistoryStore::isPlottableTelemetry(def) || !field.label) {
        return field;
    }

    // 数值型遥测：值标签 + 行内曲线按钮。按钮只放图标不放文字，靠 tooltip 说明
    auto *container = new QWidget();
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(field.label, 1);

    field.button = new QToolButton(container);
    field.button->setAutoRaise(true);
    field.button->setIcon(makePlotIcon(container->palette()));
    field.button->setIconSize(QSize(16, 16));
    field.button->setFixedSize(20, 20);
    field.button->setCursor(Qt::PointingHandCursor);
    field.button->setToolTip(tr("Show the history plot of %1")
                                 .arg(def.displayName.isEmpty() ? paramName : def.displayName));
    layout->addWidget(field.button, 0);

    connect(field.button, &QToolButton::clicked, this, [this, paramName]() {
        openHistoryWindow(paramName);
    });

    field.widget = container;
    return field;
}

void CameraTab::registerParameterField(const QString &paramName, const ParameterField &field)
{
    if (field.label) {
        m_telemetryLabels.insert(paramName, field.label);
    }
    if (field.button) {
        m_plotButtons.insert(paramName, field.button);
    }
}

void CameraTab::unregisterParameterField(const QString &paramName)
{
    m_telemetryLabels.remove(paramName);
    m_plotButtons.remove(paramName);
}

void CameraTab::updatePlotButtonIcons()
{
    for (QToolButton *button : m_plotButtons) {
        if (button) {
            button->setIcon(makePlotIcon(button->palette()));
        }
    }
}

void CameraTab::openHistoryWindow(const QString &paramName)
{
    if (!m_appController) {
        return;
    }

    // 已经开过就抬到前面，别开第二个同名窗口
    if (ParameterHistoryWindow *existing = m_historyWindows.value(paramName)) {
        existing->show();
        existing->raise();
        existing->activateWindow();
        return;
    }

    const ParameterDefinition def = m_appController->parameter(paramName);
    auto *window = new ParameterHistoryWindow(paramName, def, m_history, this);
    m_historyWindows.insert(paramName, window);
    window->show();
    window->raise();
    window->activateWindow();
}

int CameraTab::historyCapacity() const
{
    return m_history->capacity();
}

void CameraTab::setHistoryCapacity(int points)
{
    m_history->setCapacity(points);
}

void CameraTab::refreshCameraList()
{
    if (!m_appController) {
        return;
    }
    QStringList cameras = m_appController->availableCameras();
    ui->cameraComboBox->clear();
    ui->cameraComboBox->addItems(cameras);

    if (m_appController->isActive()) {
        QString currentId = m_appController->currentCameraId();
        int index = ui->cameraComboBox->findText(currentId);
        if (index >= 0) {
            ui->cameraComboBox->setCurrentIndex(index);
        }
    }

    ui->cameraComboBox->setEnabled(true);

    updateConnectionState();
}

void CameraTab::setBufferedConfig(const QVariantMap &config)
{
    m_bufferedConfig = config;
}

void CameraTab::updateBufferedConfigFromWidgets()
{
    m_bufferedConfig.clear();
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

void CameraTab::onTelemetryTimerTimeout()
{
    // 采集期间也要继续轮询：否则一开始采图，温度曲线和读数就断一段。
    // isConnected() 只认空闲状态，这里要的是「连着且在跑」的 isActive()
    if (!m_appController || !m_appController->isActive()) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QStringList paramNames = m_appController->parameterNames();

    for (const QString &paramName : paramNames) {
        const ParameterDefinition def = m_appController->parameter(paramName);
        if (!ParameterHistoryStore::isTelemetryParameter(def)) {
            continue;
        }

        // 本轮只读一次：标签和曲线用的是同一个读数
        const QVariant value = m_appController->parameterValue(paramName);

        if (QLabel *label = m_telemetryLabels.value(paramName)) {
            label->setText(value.toString());
        }

        if (ParameterHistoryStore::isPlottableTelemetry(def)) {
            bool ok = false;
            const double numericValue = value.toDouble(&ok);
            if (ok) {
                m_history->append(paramName, now, numericValue);
            }
        }
    }

    // 值都读完了再刷窗口，一轮 tick 每个窗口只重画一次
    for (ParameterHistoryWindow *window : m_historyWindows) {
        if (window && window->isVisible()) {
            window->refresh();
        }
    }
}

void CameraTab::onScanProgress(int current, int total, const QString &currentFile)
{
    Q_UNUSED(currentFile);
    if (ui->m_scanStatusLabel) {
        ui->m_scanStatusLabel->setText(
            QString("Scanning plugins... (%1/%2)").arg(current).arg(total));
        ui->m_scanStatusLabel->setVisible(true);
    }
}

void CameraTab::onScanStarted()
{
    ui->cameraComboBox->setEnabled(false);
    if (ui->m_scanStatusLabel) {
        ui->m_scanStatusLabel->setText("Scanning plugins...");
        ui->m_scanStatusLabel->setVisible(true);
    }
}

void CameraTab::onScanCompleted(int totalPlugins, int loadedPlugins)
{
    Q_UNUSED(totalPlugins);
    refreshCameraList();
    ui->cameraComboBox->setEnabled(true);

    if (ui->m_scanStatusLabel) {
        int cameraCount = m_appController ? m_appController->availableCameras().size() : 0;
        ui->m_scanStatusLabel->setText(
            QString("Scan complete: %1 plugins loaded, %2 cameras found. %3 failed.")
                .arg(loadedPlugins)
                .arg(cameraCount)
                .arg(m_lastScanFailed));
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
            QWidget *widget = m_parameterWidgets.value(paramName);
            ParameterDefinition oldDef = m_parameterDefinitions.value(paramName);

            if (widget && oldDef.constraint != def.constraint) {
                rebuildParameterWidget(paramName);
                widget = m_parameterWidgets.value(paramName);
            }

            if (widget) {
                QVariant value = m_appController->parameterValue(paramName);
                ParameterWidgetFactory::setWidgetValue(widget, value, def);
            }
        }
    }
}

void CameraTab::onSetParametersFinished(const QStringList &failedParameters)
{
    if (!failedParameters.isEmpty()) {
        if (ui && ui->m_statusLabel) {
            ui->m_statusLabel->setText(tr("Invalid parameters: %1")
                .arg(failedParameters.join(", ")));
            ui->m_statusLabel->setVisible(true);
        }
        restoreWidgetsFromConfig(m_committedConfig, failedParameters);
    }
}

void CameraTab::onCommitParametersFinished(const QStringList &failedParameters)
{
    if (ui && ui->m_statusLabel) {
        if (failedParameters.isEmpty()) {
            ui->m_statusLabel->setVisible(false);
        } else {
            ui->m_statusLabel->setText(tr("Camera rejected: %1")
                .arg(failedParameters.join(", ")));
            ui->m_statusLabel->setVisible(true);
        }
    }

    if (failedParameters.isEmpty()) {
        refreshCommittedConfigFromController();
    } else {
        restoreWidgetsFromConfig(m_committedConfig, failedParameters);
    }

    onParametersCommitted();
}

void CameraTab::restoreWidgetsFromConfig(const QVariantMap &config, const QStringList &onlyNames)
{
    const bool onlyFilter = !onlyNames.isEmpty();
    for (auto it = m_parameterWidgets.constBegin(); it != m_parameterWidgets.constEnd(); ++it) {
        const QString &name = it.key();
        if (onlyFilter && !onlyNames.contains(name)) {
            continue;
        }
        QWidget *widget = it.value();
        if (!widget || !m_parameterDefinitions.contains(name)) {
            continue;
        }
        const ParameterDefinition &def = m_parameterDefinitions.value(name);
        if (def.isReadOnly) {
            continue;
        }
        QVariant value = config.value(name, def.defaultValue);
        ParameterWidgetFactory::setWidgetValue(widget, value, def);
    }
}

void CameraTab::refreshCommittedConfigFromController()
{
    if (!m_appController) {
        return;
    }
    m_committedConfig = m_appController->allParameters();
    setBufferedConfig(m_committedConfig);
}