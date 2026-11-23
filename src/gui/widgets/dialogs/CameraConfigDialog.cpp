#include "CameraConfigDialog.h"

#include "../../AppController.h"
#include "../../Theme.h"
#include "../../services/AppSettings.h"
#include "../../services/ParameterHistoryStore.h"
#include "../common/LoadingIndicator.h"
#include "../common/ParameterWidgetFactory.h"
#include "../display/ParameterHistoryWindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLayoutItem>
#include <QPainter>
#include <QPen>
#include <QPair>
#include <QPolygonF>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVector>

#include <algorithm>

namespace
{

// How often the read-only telemetry labels are refreshed. Each tick costs one
// blocking UART round trip per dynamic parameter (~9 ms on hk16011), paid on the
// GUI thread, so the interval is deliberately coarse: a 1 s refresh is plenty
// for temperatures and acquisition state, and it leaves the GUI thread to frame
// rendering.
constexpr int kTelemetryPollMs = 1000;

//! Row-inline history button icon. The palette has no ready-made chart icon, so
//! draw a small axis + curve; updatePlotButtonIcons() redraws it on theme change.
QIcon makePlotIcon(const QPalette &palette)
{
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

QVariantMap filterDriverParams(const QVariantMap &params)
{
    QVariantMap filtered;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (it.key() == QStringLiteral("captureCount")
            || it.key() == QStringLiteral("captureMode")) {
            continue;
        }
        filtered.insert(it.key(), it.value());
    }
    return filtered;
}

} // namespace

CameraConfigDialog::CameraConfigDialog(QWidget *parent)
    : QDialog(parent)
    , m_telemetryTimer(new QTimer(this))
    , m_history(new ParameterHistoryStore(this))
{
    setWindowTitle(tr("Configuration"));
    setMinimumSize(600, 500);
    setModal(false);

    setupUi();
    loadDataSettings();

    connect(m_telemetryTimer, &QTimer::timeout,
            this, &CameraConfigDialog::onTelemetryTimerTimeout);
    connect(Theme::instance(), &Theme::themeChanged,
            this, &CameraConfigDialog::updatePlotButtonIcons);

    qApp->installEventFilter(this);
}

CameraConfigDialog::~CameraConfigDialog()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void CameraConfigDialog::setupUi()
{
    auto *root = new QVBoxLayout(this);
    m_tabs = new QTabWidget(this);

    setupCameraPage();
    setupDataPage();
    setupPluginsPage();

    root->addWidget(m_tabs, 1);
    setupLoadingRow(root);
    setupButtonBox();
    root->addWidget(m_buttonBox);
}

void CameraConfigDialog::setupCameraPage()
{
    auto *page = new QWidget(m_tabs);
    auto *form = new QFormLayout(page);
    form->setSpacing(6);
    form->setLabelAlignment(Qt::AlignRight);
    m_cameraForm = form;

    m_cameraComboBox = new QComboBox(page);
    m_cameraComboBox->setObjectName(QStringLiteral("cameraComboBox"));
    m_cameraComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_cameraComboBox->setEnabled(false);

    m_scanButton = new QPushButton(tr("Scan"), page);
    m_scanButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    auto *cameraLayout = new QHBoxLayout();
    cameraLayout->addWidget(m_scanButton);
    cameraLayout->addWidget(m_cameraComboBox, 1);
    form->addRow(tr("Camera:"), cameraLayout);

    m_connectButton = new QPushButton(tr("Connect"), page);
    m_connectButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_disconnectButton = new QPushButton(tr("Disconnect"), page);
    m_disconnectButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_disconnectButton->setEnabled(false);

    m_statusLabel = new QLabel(page);
    m_statusLabel->setObjectName(QStringLiteral("cameraStatusLabel"));
    m_statusLabel->setVisible(false);

    m_connectLoadingIndicator = new LoadingIndicator(page);
    m_connectLoadingIndicator->setFixedSize(24, 24);
    m_connectLoadingIndicator->setVisible(false);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_connectButton);
    buttonLayout->addWidget(m_disconnectButton);
    form->addRow(QString(), buttonLayout);

    auto *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(m_connectLoadingIndicator);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    form->addRow(QString(), statusLayout);

    m_scanStatusLabel = new QLabel(page);
    m_scanStatusLabel->setObjectName(QStringLiteral("scanStatusLabel"));
    m_scanStatusLabel->setVisible(false);
    form->addRow(QString(), m_scanStatusLabel);

    m_captureModeComboBox = new QComboBox(page);
    m_captureModeComboBox->setObjectName(QStringLiteral("captureModeComboBox"));
    m_captureModeComboBox->addItems({QStringLiteral("Single"), QStringLiteral("Burst"),
                                     QStringLiteral("Live")});
    m_captureModeComboBox->setCurrentText(QStringLiteral("Single"));
    m_captureModeComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_captureCountSpinBox = new QSpinBox(page);
    m_captureCountSpinBox->setObjectName(QStringLiteral("captureCountSpinBox"));
    m_captureCountSpinBox->setRange(2, 1000);
    m_captureCountSpinBox->setValue(5);
    m_captureCountSpinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    form->addRow(tr("Capture Mode:"), m_captureModeComboBox);
    m_countRow = form->rowCount();
    form->addRow(tr("Count:"), m_captureCountSpinBox);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    form->setRowVisible(m_countRow, false);
#endif

    m_parameterGroup = new QGroupBox(tr("Parameters"), page);
    m_dynamicParametersLayout = new QVBoxLayout();
    m_parameterGroup->setLayout(m_dynamicParametersLayout);
    m_parameterGroup->setVisible(false);

    auto *scrollContent = new QWidget(page);
    auto *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->addWidget(m_parameterGroup);
    scrollLayout->addStretch();

    auto *scrollArea = new QScrollArea(page);
    scrollArea->setWidget(scrollContent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    form->addRow(scrollArea);
    m_tabs->addTab(page, tr("Camera"));

    connect(m_scanButton, &QPushButton::clicked,
            this, &CameraConfigDialog::onScanButtonClicked);
    connect(m_connectButton, &QPushButton::clicked,
            this, &CameraConfigDialog::onConnectButtonClicked);
    connect(m_disconnectButton, &QPushButton::clicked,
            this, &CameraConfigDialog::onDisconnectButtonClicked);
    connect(m_cameraComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraConfigDialog::onCameraSelected);
    connect(m_captureModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraConfigDialog::onCaptureModeChanged);
}

void CameraConfigDialog::setupDataPage()
{
    auto *page = new QWidget(m_tabs);
    auto *mainLayout = new QVBoxLayout(page);
    auto *autoSaveGroup = new QGroupBox(tr("Auto-Save Settings"), page);
    auto *form = new QFormLayout();

    m_autoSaveEnabledCheckBox = new QCheckBox(tr("Enable auto-save"), page);
    m_autoSaveEnabledCheckBox->setObjectName(QStringLiteral("autoSaveEnabledCheckBox"));
    form->addRow(tr("Auto-Save:"), m_autoSaveEnabledCheckBox);

    auto *directoryLayout = new QHBoxLayout();
    m_autoSaveDirectoryLineEdit = new QLineEdit(page);
    m_autoSaveDirectoryLineEdit->setObjectName(QStringLiteral("autoSaveDirectoryLineEdit"));
    m_autoSaveDirectoryLineEdit->setPlaceholderText(tr("Select directory..."));
    m_browseDirectoryButton = new QPushButton(tr("Browse..."), page);
    directoryLayout->addWidget(m_autoSaveDirectoryLineEdit);
    directoryLayout->addWidget(m_browseDirectoryButton);
    form->addRow(tr("Directory:"), directoryLayout);

    m_imageFormatComboBox = new QComboBox(page);
    m_imageFormatComboBox->setObjectName(QStringLiteral("imageFormatComboBox"));
    m_imageFormatComboBox->addItems({QStringLiteral("TIFF"), QStringLiteral("CSV")});
    form->addRow(tr("Image Format:"), m_imageFormatComboBox);

    m_prefixLineEdit = new QLineEdit(page);
    m_prefixLineEdit->setObjectName(QStringLiteral("prefixLineEdit"));
    QRegularExpression regex(QStringLiteral("^[a-zA-Z0-9_\u4e00-\u9fa5]*$"));
    m_prefixLineEdit->setValidator(new QRegularExpressionValidator(regex, page));
    form->addRow(tr("Prefix:"), m_prefixLineEdit);

    m_suffixLineEdit = new QLineEdit(page);
    m_suffixLineEdit->setObjectName(QStringLiteral("suffixLineEdit"));
    m_suffixLineEdit->setValidator(new QRegularExpressionValidator(regex, page));
    form->addRow(tr("Suffix:"), m_suffixLineEdit);

    autoSaveGroup->setLayout(form);
    mainLayout->addWidget(autoSaveGroup);
    mainLayout->addStretch();
    m_tabs->addTab(page, tr("Data"));

    connect(m_browseDirectoryButton, &QPushButton::clicked,
            this, &CameraConfigDialog::onBrowseDirectoryClicked);
    connect(m_autoSaveEnabledCheckBox, &QCheckBox::toggled,
            this, &CameraConfigDialog::onAutoSaveToggled);
    connect(m_imageFormatComboBox, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &CameraConfigDialog::onImageFormatChanged);
    connect(m_prefixLineEdit, &QLineEdit::textChanged,
            this, &CameraConfigDialog::onPrefixChanged);
    connect(m_suffixLineEdit, &QLineEdit::textChanged,
            this, &CameraConfigDialog::onSuffixChanged);
}

void CameraConfigDialog::setupPluginsPage()
{
    auto *page = new QWidget(m_tabs);
    auto *mainLayout = new QVBoxLayout(page);

    auto *pluginDirectoryGroup = new QGroupBox(tr("Plugin Directory"), page);
    auto *directoryLayout = new QHBoxLayout();
    m_pluginDirectoryLineEdit = new QLineEdit(page);
    m_pluginDirectoryLineEdit->setObjectName(QStringLiteral("pluginDirectoryLineEdit"));
    m_pluginDirectoryLineEdit->setPlaceholderText(tr("Select plugin directory..."));
    m_browsePluginDirectoryButton = new QPushButton(tr("Browse..."), page);
    m_scanPluginsButton = new QPushButton(tr("Scan Plugins"), page);
    directoryLayout->addWidget(m_pluginDirectoryLineEdit);
    directoryLayout->addWidget(m_browsePluginDirectoryButton);
    directoryLayout->addWidget(m_scanPluginsButton);
    pluginDirectoryGroup->setLayout(directoryLayout);
    mainLayout->addWidget(pluginDirectoryGroup);

    auto *loadedPluginsGroup = new QGroupBox(tr("Available Cameras"), page);
    auto *pluginsLayout = new QVBoxLayout();

    m_pluginsTableWidget = new QTableWidget(page);
    m_pluginsTableWidget->setObjectName(QStringLiteral("pluginsTableWidget"));
    m_pluginsTableWidget->setColumnCount(2);
    m_pluginsTableWidget->setHorizontalHeaderLabels({tr("Plugin"), tr("Camera ID")});
    m_pluginsTableWidget->horizontalHeader()->setStretchLastSection(true);
    m_pluginsTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_pluginsTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    pluginsLayout->addWidget(m_pluginsTableWidget);
    m_pluginScanStatusLabel = new QLabel(page);
    m_pluginScanStatusLabel->setObjectName(QStringLiteral("pluginScanStatusLabel"));
    m_pluginScanStatusLabel->setVisible(false);
    pluginsLayout->addWidget(m_pluginScanStatusLabel);
    loadedPluginsGroup->setLayout(pluginsLayout);
    mainLayout->addWidget(loadedPluginsGroup);
    m_tabs->addTab(page, tr("Plugins"));

    const QString pluginDir = AppSettings::pluginDirectory();
    if (!pluginDir.isEmpty()) {
        m_pluginDirectoryLineEdit->setText(pluginDir);
    }

    connect(m_browsePluginDirectoryButton, &QPushButton::clicked,
            this, &CameraConfigDialog::onBrowsePluginDirectoryClicked);
    connect(m_scanPluginsButton, &QPushButton::clicked,
            this, &CameraConfigDialog::onScanPluginsClicked);
    connect(m_pluginDirectoryLineEdit, &QLineEdit::returnPressed,
            this, &CameraConfigDialog::onScanPluginsClicked);
}

void CameraConfigDialog::setupLoadingRow(QVBoxLayout *root)
{
    m_dialogLoadingIndicator = new LoadingIndicator(this);
    m_dialogLoadingIndicator->setFixedSize(24, 24);
    m_dialogLoadingIndicator->setVisible(false);

    m_loadingStatusLabel = new QLabel(this);
    m_loadingStatusLabel->setVisible(false);

    auto *loadingLayout = new QHBoxLayout();
    loadingLayout->addWidget(m_dialogLoadingIndicator);
    loadingLayout->addWidget(m_loadingStatusLabel);
    loadingLayout->addStretch();
    root->addLayout(loadingLayout);
}

void CameraConfigDialog::setupButtonBox()
{
    m_buttonBox = new QDialogButtonBox(this);
    m_buttonBox->setOrientation(Qt::Horizontal);

    auto *applyButton = new QPushButton(QDialogButtonBox::tr("Apply"), m_buttonBox);
    auto *okButton = new QPushButton(QDialogButtonBox::tr("OK"), m_buttonBox);
    auto *cancelButton = new QPushButton(QDialogButtonBox::tr("Cancel"), m_buttonBox);
    m_restoreButton = new QPushButton(QStringLiteral("Restore"), m_buttonBox);

    m_buttonBox->addButton(okButton, QDialogButtonBox::AcceptRole);
    m_buttonBox->addButton(cancelButton, QDialogButtonBox::RejectRole);
    m_buttonBox->addButton(applyButton, QDialogButtonBox::ApplyRole);
    m_buttonBox->addButton(m_restoreButton, QDialogButtonBox::ActionRole);

    // Disable auto-default on all buttons so that pressing Enter inside an
    // input field (QSpinBox, QDoubleSpinBox, QLineEdit, ...) commits the value
    // via editingFinished instead of activating the OK button and closing the
    // dialog. Buttons are still activatable by mouse click or by Tab-focusing
    // them and pressing Enter/Space.
    okButton->setAutoDefault(false);
    okButton->setDefault(false);
    cancelButton->setAutoDefault(false);
    applyButton->setAutoDefault(false);
    m_restoreButton->setAutoDefault(false);

    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &CameraConfigDialog::on_buttonBox_accepted);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &CameraConfigDialog::on_buttonBox_rejected);
    connect(m_buttonBox, &QDialogButtonBox::clicked,
            this, &CameraConfigDialog::on_buttonBox_clicked);
    connect(m_restoreButton, &QPushButton::clicked,
            this, &CameraConfigDialog::on_restoreButton_clicked);
}

void CameraConfigDialog::showDialogLoading(const QString &text)
{
    if (m_dialogLoadingIndicator) {
        m_dialogLoadingIndicator->setVisible(true);
        m_dialogLoadingIndicator->startAnimation();
    }
    if (m_loadingStatusLabel) {
        m_loadingStatusLabel->setText(text);
        m_loadingStatusLabel->setVisible(true);
    }
    if (m_buttonBox) {
        m_buttonBox->setEnabled(false);
    }
}

void CameraConfigDialog::hideDialogLoading()
{
    if (m_dialogLoadingIndicator) {
        m_dialogLoadingIndicator->stopAnimation();
        m_dialogLoadingIndicator->setVisible(false);
    }
    if (m_loadingStatusLabel) {
        m_loadingStatusLabel->setVisible(false);
    }
    if (m_buttonBox) {
        m_buttonBox->setEnabled(true);
    }
}

void CameraConfigDialog::showEvent(QShowEvent *event)
{
    if (event->type() == QEvent::Show) {
        showParameterPanel();
    }
    QDialog::showEvent(event);
}

void CameraConfigDialog::setAppController(AppController *controller)
{
    if (m_appController) {
        disconnect(m_appController, nullptr, this, nullptr);
    }
    m_appController = controller;

    if (!controller) {
        updateConnectionState();
        return;
    }

    refreshCameraList();
    // Initial state: no scan has run yet, so an empty combo must stay disabled.
    m_cameraComboBox->setEnabled(m_cameraComboBox->count() > 0);

    connect(controller, &AppController::stateChanged,
            this, &CameraConfigDialog::onCameraStateChanged);
    connect(controller, &AppController::connectCameraFinished,
            this, &CameraConfigDialog::onConnectCameraFinished);
    connect(controller, &AppController::disconnectCameraFinished,
            this, &CameraConfigDialog::onDisconnectCameraFinished);
    connect(controller, &AppController::pluginScanStarted,
            this, &CameraConfigDialog::onScanStarted);
    connect(controller, &AppController::pluginScanProgress,
            this, &CameraConfigDialog::onScanProgress);
    connect(controller, &AppController::pluginScanCompleted,
            this, &CameraConfigDialog::onScanCompleted);
    connect(controller, &AppController::pluginLoadFailed,
            this, &CameraConfigDialog::onPluginLoadFailed);
    connect(controller, &AppController::setParametersFinished,
            this, &CameraConfigDialog::onSetParametersFinished);
    connect(controller, &AppController::commitParametersFinished,
            this, &CameraConfigDialog::onCommitParametersFinished);

    if (controller->isActive()) {
        setBufferedConfig(controller->allParameters());
        m_committedConfig = controller->allParameters();
        updateConnectionState();
        buildDynamicParameterPanel();
        m_telemetryTimer->start(kTelemetryPollMs);
        if (m_parameterGroup) {
            m_parameterGroup->setVisible(true);
        }
    }

    // The plugin directory is a preference, not a per-camera setting. If it is
    // already configured, start a scan as soon as the controller arrives.
    const QString pluginDir = AppSettings::pluginDirectory();
    if (!pluginDir.isEmpty() && QDir(pluginDir).exists()) {
        m_lastScanFailed = 0;
        m_scanStatusLabel->setText(tr("Scanning plugins..."));
        m_scanStatusLabel->setVisible(true);
        m_pluginScanStatusLabel->setText(tr("Scanning plugins..."));
        m_pluginScanStatusLabel->setVisible(true);
        QMetaObject::invokeMethod(controller, &AppController::scanPlugins, Qt::QueuedConnection);
    }

    updateConnectionState();
    m_buttonBox->setEnabled(controller->state() != CameraState::Acquiring);
}

void CameraConfigDialog::updateConnectionState()
{
    if (!m_appController) {
        m_connectButton->setEnabled(false);
        m_disconnectButton->setEnabled(false);
        return;
    }

    const CameraState state = m_appController->state();
    const bool notConnected = (state == CameraState::Disconnected
                               || state == CameraState::Error);
    const bool connected = !notConnected && m_cameraComboBox->count() > 0;
    m_connectButton->setEnabled(!connected);
    m_disconnectButton->setEnabled(connected);
}

void CameraConfigDialog::refreshCameraList()
{
    if (!m_appController) {
        return;
    }

    const QStringList cameras = m_appController->availableCameras();
    m_cameraComboBox->clear();
    m_cameraComboBox->addItems(cameras);

    if (m_appController->isActive()) {
        const QString currentId = m_appController->currentCameraId();
        const int index = m_cameraComboBox->findText(currentId);
        if (index >= 0) {
            m_cameraComboBox->setCurrentIndex(index);
        }
    }

    m_cameraComboBox->setEnabled(true);
    updateConnectionState();
}

void CameraConfigDialog::onCameraSelected(int index)
{
    Q_UNUSED(index);
    updateConnectionState();
}

void CameraConfigDialog::onScanButtonClicked()
{
    startPluginScan();
}

void CameraConfigDialog::startPluginScan()
{
    if (!m_appController) {
        return;
    }

    m_lastScanFailed = 0;
    m_cameraComboBox->setEnabled(false);
    m_scanStatusLabel->setText(tr("Scanning plugins..."));
    m_scanStatusLabel->setVisible(true);
    m_pluginScanStatusLabel->setText(tr("Scanning plugins..."));
    m_pluginScanStatusLabel->setVisible(true);

    QMetaObject::invokeMethod(m_appController, &AppController::scanPlugins,
                              Qt::QueuedConnection);
}

void CameraConfigDialog::onScanPluginsClicked()
{
    const QString pluginDir = m_pluginDirectoryLineEdit->text();
    if (!pluginDir.isEmpty()) {
        AppSettings::setPluginDirectory(pluginDir);
    }
    startPluginScan();
}

void CameraConfigDialog::onBrowsePluginDirectoryClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Plugin Directory"), m_pluginDirectoryLineEdit->text());
    if (!dir.isEmpty()) {
        m_pluginDirectoryLineEdit->setText(dir);
        AppSettings::setPluginDirectory(dir);
    }
}

void CameraConfigDialog::onConnectButtonClicked()
{
    if (!m_appController || m_cameraComboBox->count() == 0) {
        return;
    }

    const QString selectedCamera = m_cameraComboBox->currentText();
    if (selectedCamera.isEmpty()) {
        return;
    }

    m_connectButton->setEnabled(false);
    m_cameraComboBox->setEnabled(false);
    if (m_connectLoadingIndicator) {
        m_connectLoadingIndicator->setVisible(true);
        m_connectLoadingIndicator->startAnimation();
    }
    m_statusLabel->setText(tr("Connecting..."));
    m_statusLabel->setVisible(true);

    QMetaObject::invokeMethod(m_appController, "connectCamera",
                              Qt::QueuedConnection,
                              Q_ARG(QString, selectedCamera));
}

void CameraConfigDialog::onDisconnectButtonClicked()
{
    if (!m_appController) {
        return;
    }
    QMetaObject::invokeMethod(m_appController, "disconnectCamera", Qt::QueuedConnection);
}

void CameraConfigDialog::onConnectCameraFinished(const QString &cameraId, bool success,
                                                 const QString &error)
{
    Q_UNUSED(cameraId);

    if (m_connectLoadingIndicator) {
        m_connectLoadingIndicator->stopAnimation();
        m_connectLoadingIndicator->setVisible(false);
    }
    m_cameraComboBox->setEnabled(true);

    if (success) {
        m_statusLabel->setText(tr("Connected"));
        m_statusLabel->setVisible(true);
        QTimer::singleShot(2000, this, [this]() {
            m_statusLabel->setVisible(false);
        });
        // New session: restart the telemetry history from a clean state. Do not
        // clear on disconnect so an open history window keeps its samples.
        m_history->clear();
    } else {
        m_statusLabel->setText(error.isEmpty() ? tr("Connection failed") : error);
        m_statusLabel->setVisible(true);
        m_connectButton->setEnabled(true);
    }

    updateConnectionState();
}

void CameraConfigDialog::onDisconnectCameraFinished(const QString &cameraId)
{
    Q_UNUSED(cameraId);
    updateConnectionState();
}

void CameraConfigDialog::onCameraStateChanged(CameraState state)
{
    if (state == CameraState::Connected) {
        if (m_parameterWidgets.isEmpty() && m_appController) {
            setBufferedConfig(m_appController->allParameters());
            buildDynamicParameterPanel();
            m_telemetryTimer->start(kTelemetryPollMs);
            if (m_parameterGroup) {
                m_parameterGroup->setVisible(true);
            }
        }
        m_buttonBox->setEnabled(true);
    } else if (state == CameraState::Disconnected || state == CameraState::Error) {
        clearDynamicParameterPanel();
        m_telemetryTimer->stop();
        if (m_parameterGroup) {
            m_parameterGroup->setVisible(false);
        }
        m_buttonBox->setEnabled(true);
    } else if (state == CameraState::Acquiring) {
        m_buttonBox->setEnabled(false);
    }

    updateConnectionState();
}

void CameraConfigDialog::onCaptureModeChanged(int index)
{
    Q_UNUSED(index);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    if (m_captureModeComboBox && m_cameraForm && m_countRow >= 0) {
        const bool isBurst = (m_captureModeComboBox->currentText() == QStringLiteral("Burst"));
        m_cameraForm->setRowVisible(m_countRow, isBurst);
    }
#endif
    emit captureModeChanged(getCaptureMode());
}

QString CameraConfigDialog::getCaptureMode() const
{
    if (!m_captureModeComboBox) {
        return QStringLiteral("Single");
    }
    return m_captureModeComboBox->currentText();
}

void CameraConfigDialog::setCaptureMode(const QString &mode)
{
    if (!m_captureModeComboBox) {
        return;
    }
    const int index = m_captureModeComboBox->findText(mode);
    if (index < 0 || m_captureModeComboBox->currentIndex() == index) {
        return;
    }
    m_captureModeComboBox->setCurrentIndex(index);
}

int CameraConfigDialog::getCaptureCount() const
{
    if (!m_captureModeComboBox) {
        return 0;
    }
    const QString mode = getCaptureMode();
    if (mode == QStringLiteral("Live")) {
        return 0;
    }
    if (mode == QStringLiteral("Single")) {
        return 1;
    }
    return m_captureCountSpinBox ? m_captureCountSpinBox->value() : 10;
}

void CameraConfigDialog::onTelemetryTimerTimeout()
{
    // Keep polling during acquisition: otherwise the temperature curves stop as
    // soon as a capture starts. isConnected() only reports the idle state, so
    // use isActive() (Connected or Acquiring) here.
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

        // Read once per tick: the label and the curve use the same value.
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

    // Refresh once after all values are in: one repaint per window per tick.
    for (ParameterHistoryWindow *window : m_historyWindows) {
        if (window && window->isVisible()) {
            window->refresh();
        }
    }
}

void CameraConfigDialog::onScanStarted()
{
    m_cameraComboBox->setEnabled(false);
    m_scanStatusLabel->setText(tr("Scanning plugins..."));
    m_scanStatusLabel->setVisible(true);
    m_pluginScanStatusLabel->setText(tr("Scanning plugins..."));
    m_pluginScanStatusLabel->setVisible(true);
}

void CameraConfigDialog::onScanProgress(int current, int total, const QString &currentFile)
{
    Q_UNUSED(currentFile);
    const QString text = tr("Scanning plugins... (%1/%2)").arg(current).arg(total);
    m_scanStatusLabel->setText(text);
    m_scanStatusLabel->setVisible(true);
    m_pluginScanStatusLabel->setText(text);
    m_pluginScanStatusLabel->setVisible(true);
}

void CameraConfigDialog::onScanCompleted(int totalPlugins, int loadedPlugins)
{
    Q_UNUSED(totalPlugins);

    updatePluginsTable();
    refreshCameraList();
    m_cameraComboBox->setEnabled(true);

    const int cameraCount = m_appController ? m_appController->availableCameras().size() : 0;
    const QString text = tr("Scan complete: %1 plugins loaded, %2 cameras found. %3 failed.")
                             .arg(loadedPlugins)
                             .arg(cameraCount)
                             .arg(m_lastScanFailed);
    m_scanStatusLabel->setText(text);
    m_scanStatusLabel->setVisible(true);
    m_pluginScanStatusLabel->setText(text);
    m_pluginScanStatusLabel->setVisible(true);
}

void CameraConfigDialog::onPluginLoadFailed(const QString &filePath, const QString &error)
{
    Q_UNUSED(filePath);
    Q_UNUSED(error);
    ++m_lastScanFailed;
    updatePluginsTable();
}

void CameraConfigDialog::updatePluginsTable()
{
    m_pluginsTableWidget->setRowCount(0);
    if (!m_appController) {
        return;
    }

    const auto plugins = m_appController->loadedPlugins();
    m_pluginsTableWidget->setRowCount(plugins.size());

    for (int i = 0; i < plugins.size(); ++i) {
        const auto &info = plugins.at(i);
        auto *pathItem = new QTableWidgetItem(QFileInfo(info.filePath).fileName());
        auto *idItem = new QTableWidgetItem(info.cameraIds.join(QStringLiteral(", ")));
        m_pluginsTableWidget->setItem(i, 0, pathItem);
        m_pluginsTableWidget->setItem(i, 1, idItem);
    }
}

void CameraConfigDialog::loadDataSettings()
{
    const QSignalBlocker lineBlocker(m_autoSaveDirectoryLineEdit);
    const QSignalBlocker checkBlocker(m_autoSaveEnabledCheckBox);
    const QSignalBlocker formatBlocker(m_imageFormatComboBox);
    const QSignalBlocker prefixBlocker(m_prefixLineEdit);
    const QSignalBlocker suffixBlocker(m_suffixLineEdit);

    m_autoSaveDirectoryLineEdit->setText(AppSettings::autoSaveDirectory());
    m_autoSaveEnabledCheckBox->setChecked(AppSettings::autoSaveEnabled());
    m_imageFormatComboBox->setCurrentText(AppSettings::imageFormat());

    m_prefixLineEdit->setText(AppSettings::filenamePrefix());
    m_suffixLineEdit->setText(AppSettings::filenameSuffix());
}

bool CameraConfigDialog::isAutoSaveEnabled() const
{
    return m_autoSaveEnabledCheckBox && m_autoSaveEnabledCheckBox->isChecked();
}

void CameraConfigDialog::setAutoSaveEnabled(bool enabled)
{
    if (!m_autoSaveEnabledCheckBox || m_autoSaveEnabledCheckBox->isChecked() == enabled) {
        return;
    }
    const QSignalBlocker blocker(m_autoSaveEnabledCheckBox);
    m_autoSaveEnabledCheckBox->setChecked(enabled);
}

QString CameraConfigDialog::autoSaveDirectory() const
{
    return m_autoSaveDirectoryLineEdit ? m_autoSaveDirectoryLineEdit->text() : QString();
}

void CameraConfigDialog::setAutoSaveDirectory(const QString &directory)
{
    if (!m_autoSaveDirectoryLineEdit
        || m_autoSaveDirectoryLineEdit->text() == directory) {
        return;
    }
    const QSignalBlocker blocker(m_autoSaveDirectoryLineEdit);
    m_autoSaveDirectoryLineEdit->setText(directory);
}

void CameraConfigDialog::onBrowseDirectoryClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Auto-Save Directory"), m_autoSaveDirectoryLineEdit->text());
    if (!dir.isEmpty()) {
        m_autoSaveDirectoryLineEdit->setText(dir);
        AppSettings::setAutoSaveDirectory(dir);
    }
}

void CameraConfigDialog::onAutoSaveToggled(bool checked)
{
    AppSettings::setAutoSaveEnabled(checked);
    emit autoSaveToggled(checked);
}

void CameraConfigDialog::onImageFormatChanged(const QString &format)
{
    AppSettings::setImageFormat(format);
}

void CameraConfigDialog::onPrefixChanged(const QString &prefix)
{
    AppSettings::setFilenamePrefix(prefix);
}

void CameraConfigDialog::onSuffixChanged(const QString &suffix)
{
    AppSettings::setFilenameSuffix(suffix);
}

void CameraConfigDialog::on_buttonBox_accepted()
{
    updateBufferedConfigFromWidgets();
    const QVariantMap bufferedConfig = getBufferedConfig();

    AppController *controller = appController();
    if (controller && controller->isConnected()) {
        const QVariantMap driverParams = filterDriverParams(bufferedConfig);
        if (!driverParams.isEmpty()) {
            showDialogLoading(tr("Applying parameters..."));
            m_acceptAfterCommit = true;
            QMetaObject::invokeMethod(controller, "setParameters",
                                      Qt::QueuedConnection,
                                      Q_ARG(QVariantMap, driverParams));
            return;
        }
    }

    accept();
}

void CameraConfigDialog::on_buttonBox_rejected()
{
    reject();
}

void CameraConfigDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    if (!button) {
        return;
    }
    if (m_buttonBox->buttonRole(button) != QDialogButtonBox::ApplyRole) {
        return;
    }

    updateBufferedConfigFromWidgets();
    const QVariantMap bufferedConfig = getBufferedConfig();

    AppController *controller = appController();
    if (controller && controller->isConnected()) {
        const QVariantMap driverParams = filterDriverParams(bufferedConfig);
        if (!driverParams.isEmpty()) {
            showDialogLoading(tr("Applying parameters..."));
            m_acceptAfterCommit = false;
            QMetaObject::invokeMethod(controller, "setParameters",
                                      Qt::QueuedConnection,
                                      Q_ARG(QVariantMap, driverParams));
        }
    }
}

void CameraConfigDialog::on_restoreButton_clicked()
{
    AppController *controller = appController();
    if (!controller || !controller->isConnected()) {
        return;
    }

    QVariantMap defaultParams;
    const QStringList paramNames = controller->parameterNames();
    for (const QString &paramName : paramNames) {
        const ParameterDefinition def = controller->parameter(paramName);
        defaultParams[paramName] = def.defaultValue;
    }

    setBufferedConfig(defaultParams);
    buildDynamicParameterPanel();
}

void CameraConfigDialog::onSetParametersFinished(const QStringList &failedParameters)
{
    if (!failedParameters.isEmpty()) {
        m_statusLabel->setText(tr("Invalid parameters: %1")
                                   .arg(failedParameters.join(QStringLiteral(", "))));
        m_statusLabel->setVisible(true);
        restoreWidgetsFromConfig(m_committedConfig, failedParameters);

        hideDialogLoading();
        if (m_acceptAfterCommit) {
            m_acceptAfterCommit = false;
            accept();
        }
        return;
    }

    AppController *controller = appController();
    if (!controller) {
        hideDialogLoading();
        return;
    }

    QMetaObject::invokeMethod(controller, "commitParameters", Qt::QueuedConnection);
}

void CameraConfigDialog::onCommitParametersFinished(const QStringList &failedParameters)
{
    if (failedParameters.isEmpty()) {
        m_statusLabel->setVisible(false);
        refreshCommittedConfigFromController();
    } else {
        m_statusLabel->setText(tr("Camera rejected: %1")
                                   .arg(failedParameters.join(QStringLiteral(", "))));
        m_statusLabel->setVisible(true);
        restoreWidgetsFromConfig(m_committedConfig, failedParameters);
    }

    onParametersCommitted();

    hideDialogLoading();
    if (m_acceptAfterCommit) {
        m_acceptAfterCommit = false;
        accept();
    }
}

bool CameraConfigDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) {
            // Only intercept Enter on input widgets that belong to this dialog
            // (avoids swallowing Enter in sub-dialogs or sibling windows).
            if (obj->isWidgetType()
                && qobject_cast<QWidget *>(obj)->window() == this
                && (qobject_cast<QLineEdit *>(obj)
                    || qobject_cast<QAbstractSpinBox *>(obj))) {
                // Commit the current value for spin boxes (QLineEdit commits on
                // every keystroke already).
                if (auto *spinBox = qobject_cast<QAbstractSpinBox *>(obj)) {
                    spinBox->interpretText();
                }
                // Emit editingFinished so listeners can react to the commit.
                QMetaObject::invokeMethod(obj, "editingFinished", Qt::DirectConnection);
                // Consume the event so it does not bubble up to the dialog and
                // activate the default button (which would close it).
                return true;
            }
        }
    }
    return QDialog::eventFilter(obj, event);
}

void CameraConfigDialog::setBufferedConfig(const QVariantMap &config)
{
    m_bufferedConfig = config;
}

QVariantMap CameraConfigDialog::getBufferedConfig() const
{
    return m_bufferedConfig;
}

void CameraConfigDialog::updateBufferedConfigFromWidgets()
{
    m_bufferedConfig.clear();
    if (m_captureModeComboBox) {
        m_bufferedConfig[QStringLiteral("captureMode")] = m_captureModeComboBox->currentText();
    }
    if (m_captureCountSpinBox) {
        m_bufferedConfig[QStringLiteral("captureCount")] = m_captureCountSpinBox->value();
        const QString captureMode = m_captureModeComboBox->currentText();
        if (captureMode == QStringLiteral("Single")) {
            m_bufferedConfig[QStringLiteral("captureCount")] = 1;
        } else if (captureMode == QStringLiteral("Live")) {
            m_bufferedConfig[QStringLiteral("captureCount")] = 0;
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

        const QVariant value = ParameterWidgetFactory::getWidgetValue(widget, defIt.value().type);
        if (value.isValid()) {
            m_bufferedConfig.insert(paramName, value);
        } else {
            qDebug() << "CameraConfigDialog: invalid value for" << paramName << ":" << value;
        }
    }
}

void CameraConfigDialog::showParameterPanel()
{
    if (!m_appController) {
        return;
    }

    setBufferedConfig(m_appController->allParameters());
    buildDynamicParameterPanel();
    if (m_parameterGroup) {
        m_parameterGroup->setVisible(true);
    }
}

void CameraConfigDialog::buildDynamicParameterPanel()
{
    if (!m_appController || !m_appController->isActive()) {
        return;
    }

    clearDynamicParameterPanel();

    const QStringList paramNames = m_appController->parameterNames();
    QMap<ParameterCategory, QVector<QPair<QString, ParameterDefinition>>> paramsByCategory;

    for (const QString &paramName : paramNames) {
        const ParameterDefinition def = m_appController->parameter(paramName);
        m_parameterDefinitions.insert(paramName, def);
        paramsByCategory[def.category].append({paramName, def});
    }

    static const QMap<ParameterCategory, QString> categoryNames = {
        {ParameterCategory::Core, QStringLiteral("Core")},
        {ParameterCategory::Cooling, QStringLiteral("Cooling")},
        {ParameterCategory::Info, QStringLiteral("Info")},
        {ParameterCategory::Advanced, QStringLiteral("Advanced")},
        {ParameterCategory::Debug, QStringLiteral("Debug")}
    };

    for (auto it = paramsByCategory.constBegin(); it != paramsByCategory.constEnd(); ++it) {
        const ParameterCategory category = it.key();
        QVector<QPair<QString, ParameterDefinition>> sortedParams = it.value();

        std::sort(sortedParams.begin(), sortedParams.end(),
                  [](const auto &a, const auto &b) {
                      return a.second.order < b.second.order;
                  });

        if (sortedParams.isEmpty()) {
            continue;
        }

        auto *categoryGroup = new QGroupBox(categoryNames.value(category),
                                            m_parameterGroup);
        auto *paramFormLayout = new QFormLayout();

        for (const auto &paramPair : sortedParams) {
            const QString &name = paramPair.first;
            const ParameterDefinition &def = paramPair.second;

            const ParameterField field = createParameterField(name, def);
            if (field.widget) {
                m_parameterWidgets.insert(name, field.widget);
                registerParameterField(name, field);
                if (m_bufferedConfig.contains(name)) {
                    ParameterWidgetFactory::setWidgetValue(field.widget,
                                                           m_bufferedConfig.value(name),
                                                           def);
                }
                paramFormLayout->addRow(def.displayName + QStringLiteral(":"), field.widget);
            }
        }

        categoryGroup->setLayout(paramFormLayout);
        m_categoryGroups.insert(category, categoryGroup);
        m_dynamicParametersLayout->addWidget(categoryGroup);
    }

    m_statusLabel->setVisible(false);
}

void CameraConfigDialog::clearDynamicParameterPanel()
{
    m_statusLabel->setVisible(false);

    // Remove the layout items now; the group boxes (and the parameter widgets
    // inside them) are deleted on the next event loop turn.
    while (QLayoutItem *item = m_dynamicParametersLayout->takeAt(0)) {
        delete item;
    }

    for (QGroupBox *group : m_categoryGroups) {
        if (group) {
            group->deleteLater();
        }
    }
    m_categoryGroups.clear();
    m_parameterWidgets.clear();
    m_parameterDefinitions.clear();
    m_telemetryLabels.clear();
    m_plotButtons.clear();
}

CameraConfigDialog::ParameterField CameraConfigDialog::createParameterField(
    const QString &paramName, const ParameterDefinition &def)
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

    // Numeric telemetry: value label + inline history button. The button is
    // icon-only; the tooltip explains it.
    auto *container = new QWidget(m_parameterGroup);
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

void CameraConfigDialog::registerParameterField(const QString &paramName,
                                                const ParameterField &field)
{
    if (field.label) {
        m_telemetryLabels.insert(paramName, field.label);
    }
    if (field.button) {
        m_plotButtons.insert(paramName, field.button);
    }
}

void CameraConfigDialog::unregisterParameterField(const QString &paramName)
{
    m_telemetryLabels.remove(paramName);
    m_plotButtons.remove(paramName);
}

void CameraConfigDialog::rebuildParameterWidget(const QString &paramName)
{
    if (!m_appController) {
        return;
    }

    const ParameterDefinition newDef = m_appController->parameter(paramName);
    QGroupBox *categoryGroup = m_categoryGroups.value(newDef.category);
    if (!categoryGroup) {
        return;
    }

    auto *formLayout = qobject_cast<QFormLayout *>(categoryGroup->layout());
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
        ParameterWidgetFactory::setWidgetValue(field.widget,
                                               m_bufferedConfig.value(paramName), newDef);
    }

    formLayout->replaceWidget(oldWidget, field.widget);
    unregisterParameterField(paramName);
    oldWidget->deleteLater();

    m_parameterWidgets.insert(paramName, field.widget);
    registerParameterField(paramName, field);
    m_parameterDefinitions.insert(paramName, newDef);
}

void CameraConfigDialog::updatePlotButtonIcons()
{
    for (QToolButton *button : m_plotButtons) {
        if (button) {
            button->setIcon(makePlotIcon(button->palette()));
        }
    }
}

void CameraConfigDialog::openHistoryWindow(const QString &paramName)
{
    if (!m_appController) {
        return;
    }

    // Reuse an existing window instead of opening a second one per parameter.
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

void CameraConfigDialog::restoreWidgetsFromConfig(const QVariantMap &config,
                                                  const QStringList &onlyNames)
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
        const QVariant value = config.value(name, def.defaultValue);
        ParameterWidgetFactory::setWidgetValue(widget, value, def);
    }
}

void CameraConfigDialog::refreshCommittedConfigFromController()
{
    if (!m_appController) {
        return;
    }
    m_committedConfig = m_appController->allParameters();
    setBufferedConfig(m_committedConfig);
}

void CameraConfigDialog::onParametersCommitted()
{
    m_statusLabel->setVisible(false);

    if (!m_appController || !m_appController->isConnected()) {
        return;
    }

    const QStringList paramNames = m_appController->parameterNames();
    for (const QString &paramName : paramNames) {
        const ParameterDefinition def = m_appController->parameter(paramName);
        if (!def.isDynamic) {
            continue;
        }

        QWidget *widget = m_parameterWidgets.value(paramName);
        const ParameterDefinition oldDef = m_parameterDefinitions.value(paramName);

        if (widget && oldDef.constraint != def.constraint) {
            rebuildParameterWidget(paramName);
            widget = m_parameterWidgets.value(paramName);
        }

        if (widget) {
            const QVariant value = m_appController->parameterValue(paramName);
            ParameterWidgetFactory::setWidgetValue(widget, value, def);
        }
    }
}
