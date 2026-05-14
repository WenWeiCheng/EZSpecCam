#include "MainWindow.h"
#include "../ui/MainWindowUi.h"
#include "../DebugMacros.h"
#include "config/CameraTab.h"
#include "display/ImageViewWidget.h"
#include "display/SpectrumViewWidget.h"
#include "CameraTypes.h"
#include "display/StatisticsDialog.h"
#include "display/ProfileWindow.h"
#include "dialogs/RowRangeDialog.h"
#include "dialogs/CustomRangeDialog.h"
#include "config/CameraConfigDialog.h"
#include "PostProcess.h"
#include "../workers/FileSaverWorker.h"

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
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <qobject.h>

Q_LOGGING_CATEGORY(parameterCategory, "Parameter")
Q_LOGGING_CATEGORY(cameraCategory, "Camera")
Q_LOGGING_CATEGORY(configCategory, "Config")
Q_LOGGING_CATEGORY(displayCategory, "Display")
Q_LOGGING_CATEGORY(captureCategory, "Capture")

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new MainWindowUi(this))
    , m_appController(nullptr)
    , m_cameraTab(nullptr)
    , m_imageViewWidget(nullptr)
    , m_spectrumViewWidget(nullptr)
    , m_fpsTimer(new QTimer(this))
    , m_launchTimestamp(QDateTime::currentDateTime())
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

    m_appController = new AppController(nullptr);

    m_imageViewWidget = ui->imageViewWidget;
    m_spectrumViewWidget = ui->spectrumViewWidget;

    connect(m_imageViewWidget, &ImageViewWidget::crosshairsCleared,
            this, &MainWindow::onCrosshairCleared);
    connect(m_imageViewWidget, &ImageViewWidget::crosshairMoved,
            this, &MainWindow::onCrosshairMoved);

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

    // Move AppController to its own dedicated thread
    m_controllerThread = new QThread(this);
    m_appController->moveToThread(m_controllerThread);
    m_controllerThread->start();

    // Call scanPlugins on the controller thread (must be after moveToThread)
    QMetaObject::invokeMethod(m_appController, &AppController::scanPlugins, Qt::QueuedConnection);

    // Set up FileSaverWorker on its own thread
    m_fileSaverThread = new QThread(this);
    m_fileSaverWorker = new FileSaverWorker();
    m_fileSaverWorker->moveToThread(m_fileSaverThread);
    m_fileSaverThread->start();

    connect(m_fileSaverWorker, &FileSaverWorker::completed,
            this, &MainWindow::onFileSaveCompleted, Qt::QueuedConnection);
    connect(m_fileSaverWorker, &FileSaverWorker::failed,
            this, &MainWindow::onFileSaveFailed, Qt::QueuedConnection);

    connect(ui->menuActionSaveFrameAs, &QAction::triggered,
            this, &MainWindow::on_actionSaveFrameAs_triggered);
    connect(ui->menuActionSaveFrame, &QAction::triggered,
            this, &MainWindow::on_actionSaveFrame_triggered);
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

    connect(ui->menuActionProfile, &QAction::triggered,
            this, &MainWindow::on_profile_triggered);

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
    if (m_fileSaverThread) {
        m_fileSaverThread->quit();
        m_fileSaverThread->wait(2000);
        delete m_fileSaverThread;
        m_fileSaverThread = nullptr;
    }

    if (m_controllerThread) {
        m_controllerThread->quit();
        m_controllerThread->wait(2000);
        delete m_controllerThread;
        m_controllerThread = nullptr;
    }
}

bool MainWindow::exportSpectrumAsCsv(const QString &filePath, bool saveOriginal) const
{
    QVector<double> xData = m_spectrumViewWidget->xData();
    QVector<double> yData = m_spectrumViewWidget->yData();

    if (xData.isEmpty() || yData.isEmpty() || !m_spectrumViewWidget->hasData()) {
        return false;
    }

    QVector<double> xCopy = xData;
    QVector<double> yCopy = yData;
    QImage origImageCopy;

    if (saveOriginal && !m_currentFrame.originalImage.isNull()) {
        origImageCopy = m_currentFrame.originalImage;
    }

    QMetaObject::invokeMethod(m_fileSaverWorker, [this, filePath, xCopy, yCopy, origImageCopy, saveOriginal]() {
        m_fileSaverWorker->exportSpectrumCsv(xCopy, yCopy, filePath);
        if (saveOriginal && !origImageCopy.isNull()) {
            QString origFilePath = filePath;
            int dotIndex = origFilePath.lastIndexOf('.');
            if (dotIndex > 0) {
                origFilePath.insert(dotIndex, "_original");
            } else {
                origFilePath += "_original";
            }
            m_fileSaverWorker->exportImageCsv(origImageCopy, origFilePath);
        }
    }, Qt::QueuedConnection);

    return true;
}

bool MainWindow::exportImageAsCsv(const QString &filePath, const QImage &image, bool saveOriginal) const
{
    if (image.isNull()) {
        return false;
    }

    QImage imageCopy = image;
    QImage origImageCopy;

    if (saveOriginal && !m_currentFrame.originalImage.isNull()) {
        origImageCopy = m_currentFrame.originalImage;
    }

    QMetaObject::invokeMethod(m_fileSaverWorker, [this, filePath, imageCopy, origImageCopy, saveOriginal]() {
        m_fileSaverWorker->exportImageCsv(imageCopy, filePath);
        if (saveOriginal && !origImageCopy.isNull()) {
            QString origFilePath = filePath;
            int dotIndex = origFilePath.lastIndexOf('.');
            if (dotIndex > 0) {
                origFilePath.insert(dotIndex, "_original");
            } else {
                origFilePath += "_original";
            }
            m_fileSaverWorker->exportImageCsv(origImageCopy, origFilePath);
        }
    }, Qt::QueuedConnection);

    return true;
}

void MainWindow::saveFrameToFile(const QString &filePath, bool isCsv)
{
    QSettings settings;
    if (isCsv) {
        int height = m_currentFrame.image.height();
        bool saveOriginal = settings.value("data/saveOriginalData", false).toBool();

        bool success = false;
        if (height == 1) {
            success = exportSpectrumAsCsv(filePath, saveOriginal);
        } else {
            success = exportImageAsCsv(filePath, m_currentFrame.image, saveOriginal);
        }

        if (!success) {
            QMessageBox::critical(this, tr("Save Error"),
                tr("Failed to save CSV to:\n%1").arg(filePath));
            return;
        }
    } else {
        ImageData frameCopy = m_currentFrame;
        bool saveMetadata = settings.value("data/saveMetadata", true).toBool();
        QMetaObject::invokeMethod(m_fileSaverWorker, [this, filePath, frameCopy, saveMetadata]() {
            m_fileSaverWorker->saveFrame(frameCopy, filePath, saveMetadata);
        }, Qt::QueuedConnection);
    }
}

void MainWindow::on_actionSaveFrameAs_triggered()
{
    if (!m_currentFrame.isValid()) {
        QMessageBox::warning(this, tr("No Frame"),
            tr("No frame available to save. Capture a frame first."));
        return;
    }

    QSettings settings;
    QString saveDir = settings.value("data/lastSaveAsDirectory").toString();
    if (saveDir.isEmpty() || !QDir(saveDir).exists()) {
        saveDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    QFileDialog dialog(this, tr("Save Frame As"), saveDir);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);

    QStringList filters;
    filters << tr("TIFF Image (*.tiff *.tif)");
    filters << tr("CSV File (*.csv)");
    dialog.setNameFilters(filters);

    QString selectedFormat = settings.value("data/imageFormat", "TIFF").toString();
    if (selectedFormat == "CSV") {
        dialog.selectNameFilter(filters.last());
    } else {
        dialog.selectNameFilter(filters.first());
    }

    QDateTime now = QDateTime::currentDateTime();
    QString ext = (selectedFormat == "CSV") ? "csv" : "tiff";
    QString prefix = settings.value("data/filenamePrefix", "").toString();
    QString suffix = settings.value("data/filenameSuffix", "").toString();
    QString prefixStr = prefix.isEmpty() ? "" : prefix + "_";
    QString suffixStr = suffix.isEmpty() ? "" : "_" + suffix;
    QString defaultName = QString("%1img_%2%3.%4").arg(prefixStr).arg(now.toString("yyyyMMdd_hhmmss")).arg(suffixStr).arg(ext);
    dialog.selectFile(defaultName);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString filePath = dialog.selectedFiles().first();
    if (filePath.isEmpty()) {
        return;
    }

    QFileInfo fileInfo(filePath);
    settings.setValue("data/lastSaveAsDirectory", fileInfo.absoluteDir().absolutePath());

    QString selectedFilter = dialog.selectedNameFilter();
    bool isCsv = selectedFilter.contains("CSV");

    saveFrameToFile(filePath, isCsv);
}

void MainWindow::on_actionSaveFrame_triggered()
{
    if (!m_currentFrame.isValid()) {
        QMessageBox::warning(this, tr("No Frame"),
            tr("No frame available to save. Capture a frame first."));
        return;
    }

    QSettings settings;
    QString saveDir = settings.value("data/autoSaveDirectory", "").toString();

    if (saveDir.isEmpty()) {
        QMessageBox::warning(this, tr("No Save Directory"),
            tr("Please set an auto-save directory first."));
        return;
    }

    QString imageFormat = settings.value("data/imageFormat", "TIFF").toString();
    bool isCsv = (imageFormat == "CSV");

    QDateTime now = QDateTime::currentDateTime();
    QString ext = isCsv ? "csv" : "tiff";
    QString prefix = settings.value("data/filenamePrefix", "").toString();
    QString suffix = settings.value("data/filenameSuffix", "").toString();
    QString prefixStr = prefix.isEmpty() ? "" : prefix + "_";
    QString suffixStr = suffix.isEmpty() ? "" : "_" + suffix;
    QString fileName = QString("%1img_%2%3.%4").arg(prefixStr).arg(now.toString("yyyyMMdd_hhmmss_zzz")).arg(suffixStr).arg(ext);
    QString filePath = saveDir + "/" + fileName;

    saveFrameToFile(filePath, isCsv);
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
    if (m_configDialog) {
        m_configDialog->show();
        m_configDialog->raise();
        m_configDialog->activateWindow();
        return;
    }
    m_configDialog = new CameraConfigDialog(this);
    m_configDialog->setAppController(m_appController);
    m_configDialog->show();
}

void MainWindow::on_actionAbout_triggered()
{
    QString aboutText = QString(
        "<h3>EZSpecCam</h3>"
        "<p>Spectral Camera Control Application</p>"
        "<p><b>Version:</b> 1.0.0</p>"
        "<p><b>Qt Version:</b> %1</p>"
        "<hr>"
        "<p>A graphical application for controlling spectral cameras "
        "and acquiring spectroscopic data.</p>"
        "<p><small>Built with Qt %1</small></p>"
    ).arg(QT_VERSION_STR);

    QMessageBox::about(this, "About EZSpecCam", aboutText);
}

void MainWindow::on_actionStart_triggered()
{
    if (!m_appController) {
        return;
    }

    int captureCount = m_configDialog ? m_configDialog->getCaptureCount() : 1;

    QMetaObject::invokeMethod(m_appController, "startCapture", Qt::QueuedConnection,
        Q_ARG(int, captureCount));
}

void MainWindow::on_actionStop_triggered()
{
    if (!m_appController) {
        return;
    }

    QMetaObject::invokeMethod(m_appController, "stopCapture", Qt::QueuedConnection);
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
    m_vBinEnabled = ui->menuActionVerticalBinning->isChecked();

    if (!m_currentFrame.isValid()) {
        return;
    }

    ImageData frame = m_currentFrame;
    if (m_vBinEnabled && frame.hasOriginal()) {
        frame.image = frame.originalImage;
        PostProcess::verticalBinning(frame, m_vBinStartRow, m_vBinEndRow);
    } else if (m_vBinEnabled) {
        PostProcess::verticalBinning(frame, m_vBinStartRow, m_vBinEndRow);
    } else if (frame.hasOriginal()) {
        frame.image = frame.originalImage;
    }
    m_currentFrame = frame;
    updateDisplay(frame);
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
    dialog->setRange(m_vBinStartRow, m_vBinEndRow < 0 ? imageHeight - 1 : m_vBinEndRow);
    dialog->show();

    connect(dialog, &RowRangeDialog::applyClicked, this, [this](int startRow, int endRow) {
        m_vBinStartRow = startRow;
        m_vBinEndRow = endRow;

        if (m_vBinEnabled && m_currentFrame.isValid()) {
            ImageData frame = m_currentFrame;
            if (frame.hasOriginal()) {
                frame.image = frame.originalImage;
            }
            PostProcess::verticalBinning(frame, m_vBinStartRow, m_vBinEndRow);
            m_currentFrame = frame;
            updateDisplay(frame);
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

    ui->frameCountLabel->setText(tr("Frames: %1").arg(++m_frameCount));
    m_fpsFrameCount++;

    if (m_vBinEnabled) {
        ImageData processed = frame;
        PostProcess::verticalBinning(processed, m_vBinStartRow, m_vBinEndRow);
        m_currentFrame = processed;
        updateDisplay(processed);
    } else {
        m_currentFrame = frame;
        updateDisplay(frame);
    }
    updateToolbarState();

    QSettings settings;
    if (settings.value("data/autoSaveEnabled", false).toBool()) {
        on_actionSaveFrame_triggered();
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
}

void MainWindow::onCaptureStopped()
{
}

void MainWindow::onCrosshairCleared()
{
    if (ui->coordLabel) {
        ui->coordLabel->setText("Crosshair: --");
    }
}

void MainWindow::onCrosshairMoved(const QPointF &position, int value)
{
    ui->coordLabel->setText(QString("Crosshair: X: %1, Y: %2, Value: %3")
                            .arg(static_cast<int>(position.x()))
                            .arg(static_cast<int>(position.y()))
                            .arg(value));

    if (m_profileWindow && m_imageViewWidget->hasImage()) {
        int x = static_cast<int>(position.x());
        int y = static_cast<int>(position.y());
        QVector<double> rowData = m_imageViewWidget->extractRowAsVector(y);
        QVector<double> colData = m_imageViewWidget->extractColumnAsVector(x);
        m_profileWindow->updateProfile(x, y, rowData, colData);
    }
}

void MainWindow::on_profile_triggered()
{
    if (!m_profileWindow) {
        m_profileWindow = new ProfileWindow(this);
        m_profileWindow->setAttribute(Qt::WA_DeleteOnClose);
    }

    if (m_imageViewWidget->hasImage()) {
        QImage img = m_imageViewWidget->image();
        m_profileWindow->setImageSize(img.width(), img.height());
    }

    m_profileWindow->show();

    if (m_imageViewWidget->crosshairCount() > 0) {
        QList<QPointF> positions = m_imageViewWidget->crosshairPositions();
        if (!positions.isEmpty()) {
            QPointF pos = positions.first();
            int x = static_cast<int>(pos.x());
            int y = static_cast<int>(pos.y());
            QVector<double> rowData = m_imageViewWidget->extractRowAsVector(y);
            QVector<double> colData = m_imageViewWidget->extractColumnAsVector(x);
            m_profileWindow->updateProfile(x, y, rowData, colData);
        }
    }
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_appController) {
        m_appController->stopCapture(100);
        m_appController->disconnectCamera();
    }
    QCoreApplication::processEvents();

    if (m_fileSaverThread) {
        m_fileSaverThread->quit();
        m_fileSaverThread->wait(2000);
        delete m_fileSaverThread;
        m_fileSaverThread = nullptr;
    }

    if (m_controllerThread) {
        m_controllerThread->quit();
        if (m_controllerThread->wait(2000)) {
            delete m_controllerThread;
            m_controllerThread = nullptr;
        }
    }

    event->accept();
}

void MainWindow::onFileSaveCompleted(const QString &path)
{
    showStatusMessage(tr("Saved: %1").arg(path), 10000);
}

void MainWindow::onFileSaveFailed(const QString &error)
{
    QMessageBox::critical(this, tr("Save Error"), error);
}