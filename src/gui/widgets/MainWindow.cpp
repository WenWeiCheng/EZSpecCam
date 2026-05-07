#include "MainWindow.h"
#include "../ui/MainWindowUi.h"
#include "CameraTab.h"
#include "ImageViewWidget.h"
#include "SpectrumViewWidget.h"
#include "CameraTypes.h"
#include "StatisticsDialog.h"
#include "RowRangeDialog.h"
#include "CustomRangeDialog.h"
#include "CameraConfigDialog.h"

#include <QMessageBox>
#include <QCloseEvent>
#include <QDebug>
#include <QTimer>
#include <QCoreApplication>
#include <QSettings>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new MainWindowUi(this))
    , m_appController(nullptr)
    , m_cameraTab(nullptr)
    , m_imageViewWidget(nullptr)
    , m_spectrumViewWidget(nullptr)
    , m_fpsTimer(new QTimer(this))
{
    ui->setupUi(this);

    ui->menuActionColorScaleAuto->setChecked(true);
    ui->menuActionSpectrumRangeAuto->setChecked(true);

    ui->centralStackedWidget->hide();

    auto *shortcutConfig = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_C), this);
    connect(shortcutConfig, &QShortcut::activated, this, &MainWindow::on_actionConfig_triggered);

    auto *shortcutLive = new QShortcut(QKeySequence(Qt::Key_L), this);
    connect(shortcutLive, &QShortcut::activated, this, &MainWindow::onLiveModeTriggered);

    auto *shortcutSingle = new QShortcut(QKeySequence(Qt::Key_S), this);
    connect(shortcutSingle, &QShortcut::activated, this, &MainWindow::onSingleModeTriggered);

    auto *shortcutBurst = new QShortcut(QKeySequence(Qt::Key_B), this);
    connect(shortcutBurst, &QShortcut::activated, this, &MainWindow::onBurstModeTriggered);

    m_appController = new AppController(this);
    m_appController->scanPlugins();

    m_imageViewWidget = ui->imageViewWidget;
    m_spectrumViewWidget = ui->spectrumViewWidget;

    connect(m_appController, &AppController::stateChanged,
            this, &MainWindow::onCameraStateChanged);
    connect(m_appController, &AppController::frameReady,
            this, &MainWindow::onCameraFrameReady);
    connect(m_appController, &AppController::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    connect(m_appController, &AppController::connectionChanged,
            this, &MainWindow::onConnectionChanged);
    connect(m_appController, &AppController::captureStarted,
            this, &MainWindow::onCaptureStarted);
    connect(m_appController, &AppController::captureStopped,
            this, &MainWindow::onCaptureStopped);

    connect(ui->menuActionSaveFrameAs, &QAction::triggered,
            this, &MainWindow::on_actionSaveFrameAs_triggered);
    connect(ui->menuActionSaveFrameAutoNumber, &QAction::triggered,
            this, &MainWindow::on_actionSaveFrameAutoNumber_triggered);
    connect(ui->menuActionAutoSaveToggle, &QAction::triggered,
            this, &MainWindow::on_actionAutoSaveToggle_triggered);
    connect(ui->menuActionChangeAutoSaveDir, &QAction::triggered,
            this, &MainWindow::on_actionChangeAutoSaveDir_triggered);
    connect(ui->menuActionConfig, &QAction::triggered,
            this, &MainWindow::on_actionConfig_triggered);
    connect(ui->menuActionAbout, &QAction::triggered,
            this, &MainWindow::on_actionAbout_triggered);

    connect(ui->menuActionColorScaleAuto, &QAction::triggered,
            this, &MainWindow::on_colorScaleAuto_triggered);
    connect(ui->menuActionColorScale8Bit, &QAction::triggered,
            this, &MainWindow::on_colorScale8Bit_triggered);
    connect(ui->menuActionColorScale16Bit, &QAction::triggered,
            this, &MainWindow::on_colorScale16Bit_triggered);

    connect(ui->menuActionSpectrumRangeAuto, &QAction::triggered,
            this, &MainWindow::on_spectrumRangeAuto_triggered);
    connect(ui->menuActionSpectrumRangeFull, &QAction::triggered,
            this, &MainWindow::on_spectrumRangeFull_triggered);
    connect(ui->menuActionSpectrumRangeZoomLeft, &QAction::triggered,
            this, &MainWindow::on_spectrumRangeZoomLeft_triggered);
    connect(ui->menuActionSpectrumRangeZoomRight, &QAction::triggered,
            this, &MainWindow::on_spectrumRangeZoomRight_triggered);
    connect(ui->menuActionSpectrumRangeZoomCenter, &QAction::triggered,
            this, &MainWindow::on_spectrumRangeZoomCenter_triggered);
    connect(ui->menuActionSpectrumRangeCustom, &QAction::triggered,
            this, &MainWindow::on_spectrumRangeCustom_triggered);

    connect(ui->menuActionShowAxes, &QAction::toggled,
            this, &MainWindow::on_showAxes_triggered);

    connect(ui->menuActionStatistics, &QAction::triggered,
            this, &MainWindow::on_statistics_triggered);

    connect(ui->menuActionVerticalBinning, &QAction::toggled,
            this, &MainWindow::on_verticalBinning_triggered);
    connect(ui->menuActionRowRange, &QAction::triggered,
            this, &MainWindow::on_rowRange_triggered);

    connect(ui->toolbarActionConfig, &QAction::triggered,
            this, &MainWindow::on_actionConfig_triggered);
    connect(ui->actionStart, &QAction::triggered,
            this, &MainWindow::on_actionStart_triggered);
    connect(ui->actionStop, &QAction::triggered,
            this, &MainWindow::on_actionStop_triggered);

    connect(m_fpsTimer, &QTimer::timeout, this, &MainWindow::onFpsTimerTimeout);

    m_frameTimer.start();
    updateToolbarState();

    QSettings settings;
    bool autoSaveEnabled = settings.value("data/autoSaveEnabled", false).toBool();
    ui->menuActionAutoSaveToggle->setChecked(autoSaveEnabled);
}

MainWindow::~MainWindow()
{
}

void MainWindow::on_actionSaveFrame_triggered()
{
    on_actionSaveFrameAutoNumber_triggered();
}

void MainWindow::on_actionSaveFrameAs_triggered()
{
    if (!m_currentFrame.isValid()) {
        QMessageBox::warning(this, tr("No Frame"),
            tr("No frame available to save. Capture a frame first."));
        return;
    }

    QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    QFileDialog dialog(this, tr("Save Frame As"), saveDir);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);

    QStringList filters;
    filters << tr("TIFF Image (*.tiff *.tif)");
    dialog.setNameFilters(filters);
    dialog.selectNameFilter(filters.first());

    QDateTime now = QDateTime::currentDateTime();
    QString defaultName = QString("frame_%1.tiff").arg(now.toString("yyyyMMdd_hhmmss"));
    dialog.selectFile(defaultName);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString filePath = dialog.selectedFiles().first();
    if (filePath.isEmpty()) {
        return;
    }

    showStatusMessage(tr("Frame saved: %1").arg(filePath), 3000);
}

void MainWindow::on_actionSaveFrameAutoNumber_triggered()
{
    if (!m_currentFrame.isValid()) {
        QMessageBox::warning(this, tr("No Frame"),
            tr("No frame available to save. Capture a frame first."));
        return;
    }

    QString fileName = QString("img_%1.tiff").arg(0, 12, 10, QChar('0'));
    showStatusMessage(tr("Frame saved: %1").arg(fileName), 3000);
}

void MainWindow::on_actionAutoSaveToggle_triggered(bool checked)
{
    QSettings settings;
    settings.setValue("data/autoSaveEnabled", checked);
    showStatusMessage(checked ? tr("Auto-save enabled") : tr("Auto-save disabled"), 2000);
}

void MainWindow::on_actionChangeAutoSaveDir_triggered()
{
    QString currentDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select Auto-Save Directory"), currentDir);

    if (dir.isEmpty()) {
        return;
    }

    QSettings settings;
    settings.setValue("data/autoSaveDirectory", dir);
    showStatusMessage(tr("Auto-save directory set to: %1").arg(dir), 3000);
}

void MainWindow::on_actionConfig_triggered()
{
    CameraConfigDialog *dialog = new CameraConfigDialog(this);
    dialog->setAppController(m_appController);
    dialog->show();
}

void MainWindow::on_actionAbout_triggered()
{
    QString aboutText = QString(
        "<h3>EZSpecCam</h3>"
        "<p>Spectral Camera Control Application</p>"
        "<p><b>Version:</b> 1.0.0</p>"
        "<p><b>Qt Version:</b> %1</p>"
    ).arg(QT_VERSION_STR);

    QMessageBox::about(this, "About EZSpecCam", aboutText);
}

void MainWindow::on_actionStart_triggered()
{
    if (!m_appController) {
        return;
    }

    m_appController->startCapture();
}

void MainWindow::on_actionStop_triggered()
{
    if (!m_appController) {
        return;
    }

    m_appController->stopCapture();
}

void MainWindow::on_colorScaleAuto_triggered()
{
    if (!m_imageViewWidget) {
        return;
    }

    m_imageViewWidget->setColorScaleMode(ImageViewWidget::ColorScaleMode::Auto);

    ui->menuActionColorScaleAuto->setChecked(true);
    ui->menuActionColorScale8Bit->setChecked(false);
    ui->menuActionColorScale16Bit->setChecked(false);
}

void MainWindow::on_colorScale8Bit_triggered()
{
    if (!m_imageViewWidget) {
        return;
    }

    m_imageViewWidget->setColorScaleMode(ImageViewWidget::ColorScaleMode::Fixed8Bit);

    ui->menuActionColorScaleAuto->setChecked(false);
    ui->menuActionColorScale8Bit->setChecked(true);
    ui->menuActionColorScale16Bit->setChecked(false);
}

void MainWindow::on_colorScale16Bit_triggered()
{
    if (!m_imageViewWidget) {
        return;
    }

    m_imageViewWidget->setColorScaleMode(ImageViewWidget::ColorScaleMode::Fixed16Bit);

    ui->menuActionColorScaleAuto->setChecked(false);
    ui->menuActionColorScale8Bit->setChecked(false);
    ui->menuActionColorScale16Bit->setChecked(true);
}

void MainWindow::on_spectrumRangeAuto_triggered()
{
    if (!m_spectrumViewWidget) {
        return;
    }

    m_spectrumViewWidget->setXAxisRangeMode(SpectrumViewWidget::XAxisRangeMode::Auto);

    ui->menuActionSpectrumRangeAuto->setChecked(true);
    ui->menuActionSpectrumRangeFull->setChecked(false);
    ui->menuActionSpectrumRangeZoomLeft->setChecked(false);
    ui->menuActionSpectrumRangeZoomRight->setChecked(false);
    ui->menuActionSpectrumRangeZoomCenter->setChecked(false);
    ui->menuActionSpectrumRangeCustom->setChecked(false);
}

void MainWindow::on_spectrumRangeFull_triggered()
{
    if (!m_spectrumViewWidget) {
        return;
    }

    m_spectrumViewWidget->setXAxisRangeMode(SpectrumViewWidget::XAxisRangeMode::Full);

    ui->menuActionSpectrumRangeAuto->setChecked(false);
    ui->menuActionSpectrumRangeFull->setChecked(true);
    ui->menuActionSpectrumRangeZoomLeft->setChecked(false);
    ui->menuActionSpectrumRangeZoomRight->setChecked(false);
    ui->menuActionSpectrumRangeZoomCenter->setChecked(false);
    ui->menuActionSpectrumRangeCustom->setChecked(false);
}

void MainWindow::on_spectrumRangeZoomLeft_triggered()
{
    if (!m_spectrumViewWidget) {
        return;
    }

    m_spectrumViewWidget->setXAxisRangeMode(SpectrumViewWidget::XAxisRangeMode::ZoomLeft);

    ui->menuActionSpectrumRangeAuto->setChecked(false);
    ui->menuActionSpectrumRangeFull->setChecked(false);
    ui->menuActionSpectrumRangeZoomLeft->setChecked(true);
    ui->menuActionSpectrumRangeZoomRight->setChecked(false);
    ui->menuActionSpectrumRangeZoomCenter->setChecked(false);
    ui->menuActionSpectrumRangeCustom->setChecked(false);
}

void MainWindow::on_spectrumRangeZoomRight_triggered()
{
    if (!m_spectrumViewWidget) {
        return;
    }

    m_spectrumViewWidget->setXAxisRangeMode(SpectrumViewWidget::XAxisRangeMode::ZoomRight);

    ui->menuActionSpectrumRangeAuto->setChecked(false);
    ui->menuActionSpectrumRangeFull->setChecked(false);
    ui->menuActionSpectrumRangeZoomLeft->setChecked(false);
    ui->menuActionSpectrumRangeZoomRight->setChecked(true);
    ui->menuActionSpectrumRangeZoomCenter->setChecked(false);
    ui->menuActionSpectrumRangeCustom->setChecked(false);
}

void MainWindow::on_spectrumRangeZoomCenter_triggered()
{
    if (!m_spectrumViewWidget) {
        return;
    }

    m_spectrumViewWidget->setXAxisRangeMode(SpectrumViewWidget::XAxisRangeMode::ZoomCenter);

    ui->menuActionSpectrumRangeAuto->setChecked(false);
    ui->menuActionSpectrumRangeFull->setChecked(false);
    ui->menuActionSpectrumRangeZoomLeft->setChecked(false);
    ui->menuActionSpectrumRangeZoomRight->setChecked(false);
    ui->menuActionSpectrumRangeZoomCenter->setChecked(true);
    ui->menuActionSpectrumRangeCustom->setChecked(false);
}

void MainWindow::on_spectrumRangeCustom_triggered()
{
    if (!m_spectrumViewWidget) {
        return;
    }

    double imageWidth = static_cast<double>(m_spectrumViewWidget->dataWidth());
    if (imageWidth <= 0) {
        imageWidth = 100.0;
    }

    double currentMin = m_spectrumViewWidget->currentXMin();
    double currentMax = m_spectrumViewWidget->currentXMax();

    CustomRangeDialog *dialog = new CustomRangeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setImageWidth(imageWidth);
    dialog->setValues(currentMin, currentMax);

    if (dialog->exec() == QDialog::Accepted) {
        double minVal = dialog->minValue();
        double maxVal = dialog->maxValue();
        m_spectrumViewWidget->setCustomXRange(minVal, maxVal);
        m_spectrumViewWidget->setXAxisRangeMode(SpectrumViewWidget::XAxisRangeMode::Custom);

        ui->menuActionSpectrumRangeAuto->setChecked(false);
        ui->menuActionSpectrumRangeFull->setChecked(false);
        ui->menuActionSpectrumRangeZoomLeft->setChecked(false);
        ui->menuActionSpectrumRangeZoomRight->setChecked(false);
        ui->menuActionSpectrumRangeZoomCenter->setChecked(false);
        ui->menuActionSpectrumRangeCustom->setChecked(true);
    } else {
        // If user cancels, keep the existing range mode
    }
}

void MainWindow::on_showAxes_triggered(bool checked)
{
    if (m_imageViewWidget) {
        m_imageViewWidget->setAxesVisible(checked);
    }
}

void MainWindow::on_statistics_triggered()
{
    if (!m_currentFrame.isValid()) {
        QMessageBox::warning(this, tr("No Frame"),
            tr("No frame available to analyze. Capture a frame first."));
        return;
    }

    StatisticsDialog *dialog = new StatisticsDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setImageData(m_currentFrame.image);
    dialog->show();
}

void MainWindow::on_verticalBinning_triggered()
{
    bool enabled = ui->menuActionVerticalBinning->isChecked();
    Q_UNUSED(enabled);

    if (m_currentFrame.isValid()) {
        updateDisplay(m_currentFrame);
    }
}

void MainWindow::on_rowRange_triggered()
{
    if (!m_currentFrame.isValid()) {
        QMessageBox::warning(this, tr("No Frame"),
            tr("No frame available. Capture a frame first to set row range."));
        return;
    }

    int imageHeight = m_currentFrame.image.height();
    RowRangeDialog *dialog = new RowRangeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setImageHeight(imageHeight);
    dialog->show();

    connect(dialog, &RowRangeDialog::applyClicked, this, [this](int startRow, int endRow) {
        // Apply row range for vertical binning
        if (m_appController) {
            QVariantMap params;
            params["rowStart"] = startRow;
            params["rowEnd"] = endRow;
            m_appController->setParameters(params);
            m_appController->commitParameters();
        }
    });
}

void MainWindow::onCameraStateChanged(CameraState newState)
{
    QString stateText;
    switch (newState) {
    case CameraState::Disconnected:
        stateText = tr("Disconnected");
        m_fpsTimer->stop();
        m_fpsFrameCount = 0;
        m_fpsValue = 0;
        updateFpsDisplay();
        break;
    case CameraState::Connecting:
        stateText = tr("Connecting...");
        break;
    case CameraState::Connected:
        stateText = tr("Connected");
        break;
    case CameraState::Acquiring:
        stateText = tr("Acquiring");
        m_fpsFrameCount = 0;
        m_fpsValue = 0;
        m_fpsTimer->start(1000);
        updateFpsDisplay();
        break;
    case CameraState::Error:
        stateText = tr("Error");
        m_fpsTimer->stop();
        break;
    }
    ui->stateLabel->setText(stateText);

    updateToolbarState();
}

void MainWindow::onCameraFrameReady(const ImageData &frame)
{
    m_currentFrame = frame;

    m_fpsFrameCount++;

    updateDisplay(frame);
    updateToolbarState();

    QSettings settings;
    if (settings.value("data/autoSaveEnabled", false).toBool()) {
        QString saveDir = settings.value("data/autoSaveDirectory",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            + "/EZSpecCamData").toString();

        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd-hh-mm-ss");
        QString fullDir = saveDir + "/" + timestamp;
        QDir().mkpath(fullDir);

        int frameNum = ++m_autoSaveFrameCounter;
        QString filePath = QString("%1/img_%2.tiff")
            .arg(fullDir)
            .arg(frameNum, 12, 10, QChar('0'));
        if (frame.image.save(filePath, "TIFF")) {
            showStatusMessage(tr("Auto-saved frame %1").arg(frameNum), 2000);
        }
    }
}

void MainWindow::onConnectionChanged(bool connected)
{
    Q_UNUSED(connected);
    updateToolbarState();
}

void MainWindow::onErrorOccurred(const CameraError &error)
{
    QString message = error.description.isEmpty()
        ? tr("An unspecified camera error occurred (code: %1)").arg(static_cast<int>(error.code))
        : error.description;
    QMessageBox::warning(this, tr("Camera Error"), message);
}

void MainWindow::onCaptureStarted()
{
    ui->frameCountLabel->setText(tr("Frames: %1").arg(++m_frameCount));
}

void MainWindow::onCaptureStopped()
{
}

void MainWindow::onLiveModeTriggered()
{
    if (m_appController) {
        m_appController->startCapture(0);
        showStatusMessage("Mode: Live", 3000);
    }
}

void MainWindow::onSingleModeTriggered()
{
    if (m_appController) {
        m_appController->startCapture(1);
        showStatusMessage("Mode: Single", 3000);
    }
}

void MainWindow::onBurstModeTriggered()
{
    if (m_appController) {
        m_appController->startCapture(5);
        showStatusMessage("Mode: Burst (5 frames)", 3000);
    }
}

void MainWindow::updateToolbarState()
{
    if (!m_appController) {
        ui->actionConfig->setEnabled(false);
        ui->toolbarActionConfig->setEnabled(false);
        ui->actionStart->setEnabled(false);
        ui->actionStop->setEnabled(false);
        return;
    }

    const bool connected = m_appController->isConnected();
    const bool acquiring = m_appController->state() == CameraState::Acquiring;

    ui->actionConfig->setEnabled(!acquiring);
    ui->toolbarActionConfig->setEnabled(!acquiring);
    ui->actionStart->setEnabled(connected && !acquiring);
    ui->actionStop->setEnabled(acquiring);
}

void MainWindow::updateDisplay(const ImageData &frame)
{
    if (!frame.isValid()) {
        return;
    }

    if (!m_frameTimer.hasExpired(MIN_FRAME_INTERVAL_MS)) {
        return;
    }

    m_frameTimer.restart();

    const int height = frame.image.height();
    switchView(height);

    if (height == 1) {
        m_spectrumViewWidget->setFromImage(frame.image);
    } else {
        m_imageViewWidget->setImage(frame.image);
    }
}

void MainWindow::switchView(int height)
{
    if (!ui->centralStackedWidget->isVisible()) {
        ui->centralStackedWidget->show();
    }

    if (height == 1) {
        ui->centralStackedWidget->setCurrentWidget(m_spectrumViewWidget);
        QCoreApplication::processEvents();
        m_spectrumViewWidget->resize(ui->centralStackedWidget->size());
        ui->menuActionColorScale->setEnabled(false);
        ui->menuActionSpectrumRange->setEnabled(true);
    } else {
        ui->centralStackedWidget->setCurrentWidget(m_imageViewWidget);
        QCoreApplication::processEvents();
        ui->menuActionColorScale->setEnabled(true);
        ui->menuActionSpectrumRange->setEnabled(false);
    }
}

void MainWindow::updateFpsDisplay()
{
    if (ui->fpsLabel) {
        if (m_fpsValue > 0) {
            ui->fpsLabel->setText(QString("FPS: %1").arg(m_fpsValue));
        } else {
            ui->fpsLabel->setText("FPS: 0");
        }
    }
}

void MainWindow::onFpsTimerTimeout()
{
    m_fpsValue = m_fpsFrameCount;
    m_fpsFrameCount = 0;
    updateFpsDisplay();
}

void MainWindow::showStatusMessage(const QString &message, int timeoutMs)
{
    if (statusBar()) {
        statusBar()->showMessage(message, timeoutMs);
    }
}