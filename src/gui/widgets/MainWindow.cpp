#include "MainWindow.h"
#include "../ui/MainWindowUi.h"
#include "../DebugMacros.h"
#include "../Theme.h"
#include "config/CameraTab.h"
#include "display/ImageViewWidget.h"
#include "display/SpectrumViewWidget.h"
#include "CameraTypes.h"
#include "display/StatisticsDialog.h"
#include "display/ProfileWindow.h"
#include "display/HistogramWindow.h"
#include "dialogs/RowRangeDialog.h"
#include "dialogs/CalibrationDialog.h"
#include "dialogs/ScaleControlDialog.h"
#include "dialogs/DisplayStyleDialog.h"
#include "config/CameraConfigDialog.h"
#include "config/DataTab.h"
#include "../ui/CameraConfigDialogUi.h"
#include "PostProcess.h"
#include "../workers/FileSaverWorker.h"
#include "../workers/FileLoaderWorker.h"
#include "formats/SaveTypes.h"
#include "formats/CsvFormatHandler.h"
#include "formats/TiffFormatHandler.h"

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
    // 必须在 setupUi 之前：绘图控件是在那里创建的，创建时就会按当前主题取色
    restoreTheme();

    ui->setupUi(this);

    ui->centralStackedWidget->hide();
    shortcutLive = new QShortcut(QKeySequence(Qt::Key_L), this);
    connect(shortcutLive, &QShortcut::activated, this, &MainWindow::onLiveModeTriggered);

    shortcutSingle = new QShortcut(QKeySequence(Qt::Key_S), this);
    connect(shortcutSingle, &QShortcut::activated, this, &MainWindow::onSingleModeTriggered);

    shortcutBurst = new QShortcut(QKeySequence(Qt::Key_B), this);
    connect(shortcutBurst, &QShortcut::activated, this, &MainWindow::onBurstModeTriggered);


    m_appController = new AppController(nullptr);

    m_imageViewWidget = ui->imageViewWidget;
    m_spectrumViewWidget = ui->spectrumViewWidget;

    m_scaleDialog = new ScaleControlDialog(this);
    m_scaleDialog->setImageScaleType(0);
    m_scaleDialog->setImageColorScaleMode(0);
    m_scaleDialog->setSpectrumScaleType(0);
    m_scaleDialog->setSpectrumXRangeMode(0);
    m_scaleDialog->setSpectrumYRangeMode(0);

    connect(m_scaleDialog, &ScaleControlDialog::imageScaleTypeChanged,
            this, [this](int type) {
                if (m_imageViewWidget) {
                    m_imageViewWidget->setIntensityScaleType(
                        type == 0 ? ImageViewWidget::IntensityScaleType::Linear
                                  : ImageViewWidget::IntensityScaleType::Log);
                }
            });

    connect(m_scaleDialog, &ScaleControlDialog::imageColorScaleModeChanged,
            this, [this](int mode) {
                if (m_imageViewWidget) {
                    switch (mode) {
                        case 0: m_imageViewWidget->setColorScaleMode(ImageViewWidget::ColorScaleMode::Auto); break;
                        case 1: m_imageViewWidget->setColorScaleMode(ImageViewWidget::ColorScaleMode::Fixed8Bit); break;
                        case 2: m_imageViewWidget->setColorScaleMode(ImageViewWidget::ColorScaleMode::Fixed16Bit); break;
                    }
                }
            });

    connect(m_scaleDialog, &ScaleControlDialog::spectrumScaleTypeChanged,
            this, [this](int type) {
                if (m_spectrumViewWidget) {
                    m_spectrumViewWidget->setIntensityScaleType(
                        type == 0 ? SpectrumViewWidget::IntensityScaleType::Auto
                                  : SpectrumViewWidget::IntensityScaleType::Log);
                }
            });

    connect(m_scaleDialog, &ScaleControlDialog::spectrumXRangeModeChanged,
            this, [this](int mode) {
                if (m_spectrumViewWidget) {
                    if (mode == 1) {
                        m_scaleDialog->setSpectrumManualXRange(
                            m_spectrumViewWidget->currentXMin(),
                            m_spectrumViewWidget->currentXMax());
                        m_spectrumViewWidget->setManualXRange(
                            m_spectrumViewWidget->currentXMin(),
                            m_spectrumViewWidget->currentXMax());
                    }
                    m_spectrumViewWidget->setXAxisRangeMode(
                        mode == 0 ? SpectrumViewWidget::AxisRangeMode::Auto
                                  : SpectrumViewWidget::AxisRangeMode::Manual);
                }
            });

    connect(m_scaleDialog, &ScaleControlDialog::spectrumYRangeModeChanged,
            this, [this](int mode) {
                if (m_spectrumViewWidget) {
                    if (mode == 1) {
                        m_scaleDialog->setSpectrumManualYRange(
                            m_spectrumViewWidget->currentYMin(),
                            m_spectrumViewWidget->currentYMax());
                        m_spectrumViewWidget->setManualYRange(
                            m_spectrumViewWidget->currentYMin(),
                            m_spectrumViewWidget->currentYMax());
                    }
                    m_spectrumViewWidget->setYAxisRangeMode(
                        mode == 0 ? SpectrumViewWidget::AxisRangeMode::Auto
                                  : SpectrumViewWidget::AxisRangeMode::Manual);
                }
            });

    connect(m_scaleDialog, &ScaleControlDialog::spectrumManualXRangeChanged,
            this, [this](double min, double max) {
                if (m_spectrumViewWidget) {
                    m_spectrumViewWidget->setManualXRange(min, max);
                }
            });

    connect(m_scaleDialog, &ScaleControlDialog::spectrumManualYRangeChanged,
            this, [this](double min, double max) {
                if (m_spectrumViewWidget) {
                    m_spectrumViewWidget->setManualYRange(min, max);
                }
            });

    m_displayStyleDialog = new DisplayStyleDialog(this);
    m_displayStyleDialog->setImageColorMap(0);
    m_displayStyleDialog->setSpectrumLineStyle(0);

    connect(m_displayStyleDialog, &DisplayStyleDialog::colorScaleToggled,
            this, [this](bool visible) {
                if (m_imageViewWidget) {
                    m_imageViewWidget->setColorScaleVisible(visible);
                }
            });

    connect(m_displayStyleDialog, &DisplayStyleDialog::imageColorMapChanged,
            this, [this](int map) {
                if (m_imageViewWidget) {
                    m_imageViewWidget->setColorMap(static_cast<ImageViewWidget::ColorMap>(map));
                }
            });

    connect(m_displayStyleDialog, &DisplayStyleDialog::spectrumLineStyleChanged,
            this, [this](int style) {
                if (m_spectrumViewWidget) {
                    m_spectrumViewWidget->setLineStyle(
                        static_cast<SpectrumViewWidget::LineStyle>(style));
                }
            });

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

    m_fileSaverWorker->registerHandler(std::make_unique<CsvFormatHandler>());
    m_fileSaverWorker->registerHandler(std::make_unique<TiffFormatHandler>());

    // Set up FileLoaderWorker on its own thread
    m_fileLoaderThread = new QThread(this);
    m_fileLoaderWorker = new FileLoaderWorker();
    m_fileLoaderWorker->moveToThread(m_fileLoaderThread);
    m_fileLoaderThread->start();

    connect(m_fileLoaderWorker, &FileLoaderWorker::frameLoaded,
            this, &MainWindow::onFrameLoaded, Qt::QueuedConnection);
    connect(m_fileLoaderWorker, &FileLoaderWorker::loadFailed,
            this, &MainWindow::onFrameLoadFailed, Qt::QueuedConnection);

    connect(ui->menuActionSaveFrameAs, &QAction::triggered,
            this, &MainWindow::on_actionSaveFrameAs_triggered);
    connect(ui->menuActionSaveFrame, &QAction::triggered,
            this, &MainWindow::on_actionSaveFrame_triggered);
    connect(ui->menuActionAutoSaveToggle, &QAction::triggered,
            this, &MainWindow::on_actionAutoSaveToggle_triggered);
    connect(ui->menuActionChangeAutoSaveDir, &QAction::triggered,
            this, &MainWindow::on_actionChangeAutoSaveDir_triggered);
    connect(ui->menuActionOpenFrame, &QAction::triggered,
            this, &MainWindow::on_actionOpenFrame_triggered);
    connect(ui->menuActionConfig, &QAction::triggered,
            this, &MainWindow::on_actionConfig_triggered);
    connect(ui->menuActionAbout, &QAction::triggered,
            this, &MainWindow::on_actionAbout_triggered);

    connect(ui->menuActionShowAxes, &QAction::toggled,
            this, &MainWindow::on_showAxes_triggered);

    connect(ui->menuActionThemeSystem, &QAction::triggered,
            this, [this] { setThemeMode(Theme::Mode::System); });
    connect(ui->menuActionThemeLight, &QAction::triggered,
            this, [this] { setThemeMode(Theme::Mode::Light); });
    connect(ui->menuActionThemeDark, &QAction::triggered,
            this, [this] { setThemeMode(Theme::Mode::Dark); });
    syncThemeMenu();

    connect(ui->menuActionFillWindow, &QAction::toggled,
            this, &MainWindow::on_fillWindow_triggered);

    connect(ui->menuActionStatistics, &QAction::triggered,
            this, &MainWindow::on_statistics_triggered);

    connect(ui->menuActionVerticalBinning, &QAction::toggled,
            this, &MainWindow::on_verticalBinning_triggered);
    connect(ui->menuActionRowRange, &QAction::triggered,
            this, &MainWindow::on_rowRange_triggered);

    connect(ui->menuActionCalibration, &QAction::triggered,
            this, &MainWindow::on_actionCalibration_triggered);

    connect(ui->menuActionProfile, &QAction::triggered,
            this, &MainWindow::on_profile_triggered);

    connect(ui->menuActionHistogram, &QAction::triggered,
            this, &MainWindow::on_histogram_triggered);

    connect(ui->menuActionScale, &QAction::triggered,
            this, &MainWindow::on_scale_triggered);

    connect(ui->menuActionDisplayStyle, &QAction::triggered,
            this, &MainWindow::on_display_style_triggered);

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

    if (m_fileLoaderThread) {
        m_fileLoaderThread->quit();
        m_fileLoaderThread->wait(2000);
        delete m_fileLoaderThread;
        m_fileLoaderThread = nullptr;
    }

    if (m_controllerThread) {
        m_controllerThread->quit();
        m_controllerThread->wait(2000);
        delete m_controllerThread;
        m_controllerThread = nullptr;
    }
}

void MainWindow::saveFrameToFile(const QString &filePath)
{
    SaveRequest request;
    request.frame = m_currentFrame;

    // 新格式：始终保存 original 2D 图 + metadata。
    // softwareSettings 仅在 vbin 范围是图像行数的真子集时写入
    // （加载时据此自动重算 spectrum）；无效范围直接不写。
    const int h = request.frame.image.height();
    if (h > 0) {
        const int effEnd = (m_vBinEndRow < 0) ? (h - 1) : m_vBinEndRow;
        const bool meaningfulRange = m_vBinEnabled
            && (m_vBinStartRow > 0 || effEnd < h - 1);
        if (meaningfulRange) {
            request.frame.softwareSettings["softwareVerticalBinning"] = true;
            request.frame.softwareSettings["vBinStartRow"] = m_vBinStartRow;
            request.frame.softwareSettings["vBinEndRow"] = effEnd;
        } else {
            request.frame.softwareSettings["softwareVerticalBinning"] = false;
        }
    }

    request.filePath = filePath;
    request.options = SaveOptions{};

    QMetaObject::invokeMethod(m_fileSaverWorker, [this, request]() {
        m_fileSaverWorker->saveFrame(request);
    }, Qt::QueuedConnection);
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

    dialog.setNameFilters(m_fileSaverWorker->availableFormatNames());

    QDateTime now = QDateTime::currentDateTime();
    QString prefix = settings.value("data/filenamePrefix", "").toString();
    QString suffix = settings.value("data/filenameSuffix", "").toString();
    QString imageFormat = settings.value("data/imageFormat", "TIFF").toString();
    QString ext = imageFormat.toLower();
    QString prefixStr = prefix.isEmpty() ? "" : prefix + "_";
    QString suffixStr = suffix.isEmpty() ? "" : "_" + suffix;
    QString defaultName = QString("%1img_%2%3.%4").arg(prefixStr).arg(now.toString("yyyyMMdd_hhmmss_zzz")).arg(suffixStr).arg(ext);
    dialog.selectFile(defaultName);

    if (imageFormat == QStringLiteral("TIFF")) {
        dialog.selectNameFilter(QStringLiteral("TIFF Image (*.tiff *.tif)"));
    } else if (imageFormat == QStringLiteral("CSV")) {
        dialog.selectNameFilter(QStringLiteral("CSV File (*.csv)"));
    }

    if (!dialog.exec() || dialog.selectedFiles().isEmpty()) {
        return;
    }

    QString filePath = dialog.selectedFiles().first();
    QFileInfo fileInfo(filePath);
    settings.setValue("data/lastSaveAsDirectory", fileInfo.absoluteDir().absolutePath());

    saveFrameToFile(filePath);
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

    QDateTime now = QDateTime::currentDateTime();
    QString prefix = settings.value("data/filenamePrefix", "").toString();
    QString suffix = settings.value("data/filenameSuffix", "").toString();
    QString imageFormat = settings.value("data/imageFormat", "TIFF").toString();
    QString ext = imageFormat.toLower();
    QString prefixStr = prefix.isEmpty() ? "" : prefix + "_";
    QString suffixStr = suffix.isEmpty() ? "" : "_" + suffix;
    QString fileName = QString("%1img_%2%3.%4").arg(prefixStr).arg(now.toString("yyyyMMdd_hhmmss_zzz")).arg(suffixStr).arg(ext);
    QString filePath = saveDir + "/" + fileName;

    saveFrameToFile(filePath);
}

void MainWindow::on_actionAutoSaveToggle_triggered(bool checked)
{
    QSettings settings;
    settings.setValue("data/autoSaveEnabled", checked);
    if (m_configDialog && m_configDialog->getUi() && m_configDialog->getUi()->dataTab
        && m_configDialog->getUi()->dataTab->isAutoSaveEnabled() != checked) {
        m_configDialog->getUi()->dataTab->setAutoSaveEnabled(checked);
    }
    showStatusMessage(checked ? tr("Auto-save enabled") : tr("Auto-save disabled"), 2000);
}

void MainWindow::on_actionChangeAutoSaveDir_triggered()
{
    QSettings settings;
    QString currentDir = settings.value("data/autoSaveDirectory").toString();
    if (currentDir.isEmpty() || !QDir(currentDir).exists()) {
        currentDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    QString dir = QFileDialog::getExistingDirectory(this,
        tr("Select Auto-Save Directory"), currentDir);

    if (dir.isEmpty()) {
        return;
    }

    settings.setValue("data/autoSaveDirectory", dir);
    if (m_configDialog && m_configDialog->getUi() && m_configDialog->getUi()->dataTab
        && m_configDialog->getUi()->dataTab->autoSaveDirectory() != dir) {
        m_configDialog->getUi()->dataTab->setAutoSaveDirectory(dir);
    }
    showStatusMessage(tr("Auto-save directory set to: %1").arg(dir), 3000);
}

void MainWindow::on_actionOpenFrame_triggered()
{
    QSettings settings;
    QString openDir = settings.value("data/lastOpenDirectory").toString();
    if (openDir.isEmpty() || !QDir(openDir).exists()) {
        openDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    QFileDialog dialog(this, tr("Open Frame"), openDir);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(QStringList{
        FileLoaderWorker::openFormatsDisplayName(),
        QStringLiteral("TIFF Image (*.tiff *.tif)"),
        QStringLiteral("CSV File (*.csv)"),
        QStringLiteral("All Files (*)")
    });

    if (!dialog.exec() || dialog.selectedFiles().isEmpty()) {
        return;
    }

    QString filePath = dialog.selectedFiles().first();
    QFileInfo fileInfo(filePath);
    settings.setValue("data/lastOpenDirectory", fileInfo.absoluteDir().absolutePath());

    QMetaObject::invokeMethod(m_fileLoaderWorker, "loadFrame",
        Qt::QueuedConnection,
        Q_ARG(QString, filePath));
    showStatusMessage(tr("Loading frame: %1").arg(fileInfo.fileName()), 2000);
}

void MainWindow::onFrameLoaded(const LoadResult &result, const QString &filePath)
{
    if (!result.success) {
        QMessageBox::warning(this, tr("Open Frame"),
            tr("Failed to load frame: %1").arg(result.errorMessage));
        return;
    }
    if (m_loadingFrameIsDark) {
        m_loadingFrameIsDark = false;
        m_darkFrame = result.frame.hasOriginal()
            ? result.frame.originalImage
            : result.frame.image;
        m_darkFrameValid = !m_darkFrame.isNull();
        showStatusMessage(m_darkFrameValid
            ? tr("Dark frame loaded.")
            : tr("Failed to load dark frame: empty image."), 5000);
        return;
    }


    // 旧格式检测：主文件若为 1 行 spectrum，且无 _original 边车，
    // 则视为已废弃的旧格式（旧格式允许保存 1 行 spectrum 作为主文件），
    // 直接拒绝加载。
    if (result.frame.image.height() == 1 && result.frame.originalImage.isNull()) {
        QMessageBox::warning(this, tr("Open Frame"),
            tr("Failed to load %1: deprecated legacy format (1-row spectrum as main file "
               "is no longer supported; please re-capture).")
                .arg(QFileInfo(filePath).fileName()));
        return;
    }

    m_currentFrame = result.frame;

    const QImage &img = m_currentFrame.image;
    const int imgHeight = img.height();
    const QVariantMap &sw = m_currentFrame.softwareSettings;

    // 自动恢复 spectrum：仅当 metadata 含 vbin 范围，且该范围是图像行数的真子集时
    const int savedStart = sw.value("vBinStartRow", -1).toInt();
    const int savedEnd = sw.value("vBinEndRow", -1).toInt();
    const bool hasRange = (savedStart >= 0) && (savedEnd >= 0);
    const bool fullRange = hasRange && (savedStart == 0) && (savedEnd >= imgHeight - 1);

    const bool shouldRecover = result.hasMetadata
        && hasRange
        && !fullRange
        && imgHeight > 1;

    if (shouldRecover) {
        m_vBinStartRow = qBound(0, savedStart, imgHeight - 1);
        m_vBinEndRow = qBound(m_vBinStartRow, savedEnd, imgHeight - 1);
        m_vBinEnabled = true;

        PostProcess::verticalBinning(m_currentFrame, m_vBinStartRow, m_vBinEndRow);
    } else {
        // 缺 metadata / 无 vbin 范围 / 范围覆盖整图 → 直接显示 2D 图
        m_vBinEnabled = false;
        if (hasRange) {
            m_vBinStartRow = qBound(0, savedStart, qMax(0, imgHeight - 1));
            m_vBinEndRow = (savedEnd < 0)
                ? qMax(0, imgHeight - 1)
                : qBound(m_vBinStartRow, savedEnd, qMax(0, imgHeight - 1));
        } else {
            m_vBinStartRow = 0;
            m_vBinEndRow = (imgHeight > 0) ? (imgHeight - 1) : -1;
        }
    }

    ui->menuActionVerticalBinning->blockSignals(true);
    ui->menuActionVerticalBinning->setChecked(m_vBinEnabled);
    ui->menuActionVerticalBinning->blockSignals(false);

    updateDisplay(m_currentFrame);

    QString msg = tr("Loaded %1").arg(QFileInfo(filePath).fileName());
    QStringList parts;
    if (result.hasMetadata) parts << tr("with metadata");
    if (shouldRecover) parts << tr("spectrum recovered");
    if (!parts.isEmpty()) msg += QStringLiteral(" (") + parts.join(QStringLiteral(", ")) + QStringLiteral(")");
    showStatusMessage(msg, 3000);
}

void MainWindow::onFrameLoadFailed(const QString &error, const QString &filePath)
{
    QMessageBox::warning(this, tr("Open Frame"),
        tr("Failed to load %1: %2").arg(QFileInfo(filePath).fileName(), error));
}

void MainWindow::on_actionConfig_triggered()
{
    if (m_configDialog) {
        m_configDialog->setModal(false);
        m_configDialog->show();
        m_configDialog->raise();
        m_configDialog->activateWindow();
        return;
    }
    m_configDialog = new CameraConfigDialog(this);
    m_configDialog->setAppController(m_appController);
    m_configDialog->setModal(false);
    if (auto *dataTab = m_configDialog->getUi()->dataTab) {
        connect(dataTab, &DataTab::autoSaveToggled,
                ui->menuActionAutoSaveToggle, &QAction::setChecked);
    }
    m_configDialog->show();
}
void MainWindow::on_actionCalibration_triggered()
{
    if (!m_calibrationDialog) {
        m_calibrationDialog = new CalibrationDialog(this);
        connect(m_calibrationDialog, &CalibrationDialog::applied,
                this, &MainWindow::onCalibrationApplied);
        connect(m_calibrationDialog, &CalibrationDialog::acquireRequested,
                this, &MainWindow::onAcquireDarkFrameStartRequested);
    }

    refreshCalibrationDialog();
    m_calibrationDialog->show();
    m_calibrationDialog->raise();
    m_calibrationDialog->activateWindow();
}

// 窗口开着的时候状态也得跟着变：采完暗帧当场把「已经采到 N 帧平均」显示出来，
// 用户不用关掉再打开就能勾 Enable。
void MainWindow::refreshCalibrationDialog()
{
    if (!m_calibrationDialog) {
        return;
    }

    m_calibrationDialog->setDarkFrameEnabled(m_darkEnabled);
    m_calibrationDialog->setDarkFramePath(m_darkPath);
    // If no file is selected but an acquired dark frame is already in memory,
    // surface that in the dialog so the user sees what is actually in use.
    // The path field stays empty (placeholder only), so darkFramePath() keeps
    // returning "" and we never write the indicator into m_darkPath.
    m_calibrationDialog->setInMemoryDarkFrameUsed(
        m_darkPath.isEmpty() && m_darkFrameValid, m_darkBurstTotal);
    m_calibrationDialog->setCustomBias(m_darkBias);
    m_calibrationDialog->setFrameCount(m_darkBurstTotal);
    m_calibrationDialog->setAcquireInProgress(m_acquiringDark);

    const bool connected = m_appController && m_appController->isConnected();
    const bool capturing = connected && m_appController->state() == CameraState::Acquiring;
    m_calibrationDialog->setAcquireEnabled(connected && !capturing && !m_acquiringDark);
}

void MainWindow::onCalibrationApplied(bool enabled, const QString &path, int bias)
{
    m_darkEnabled = enabled;
    m_darkPath = path;
    m_darkBias = bias;

    if (enabled && !m_darkFrameValid && !path.isEmpty()) {
        m_loadingFrameIsDark = true;
        QMetaObject::invokeMethod(m_fileLoaderWorker, "loadFrame",
            Qt::QueuedConnection, Q_ARG(QString, path));
    }

    showStatusMessage(enabled
        ? tr("Dark-frame calibration enabled.")
        : tr("Dark-frame calibration disabled."), 3000);
}

void MainWindow::onAcquireDarkFrameStartRequested(int frameCount)
{
    if (m_acquiringDark) {
        showStatusMessage(tr("Dark-frame acquisition already in progress."), 2000);
        return;
    }
    int n = frameCount;
    if (n < 1) n = 1;
    if (n > 1000) n = 1000;

    m_darkBurstTotal = n;
    m_darkBurstRemaining = n;
    m_darkAccumInit = false;
    m_darkAccumFrames = 0;
    m_darkAccumSum.clear();

    QMetaObject::invokeMethod(m_appController, "startCapture",
        Qt::QueuedConnection, Q_ARG(int, n));

    m_acquiringDark = true;
    if (m_calibrationDialog) {
        m_calibrationDialog->setAcquireInProgress(true);
    }

    showStatusMessage(tr("Acquiring dark frame (%1/%2)...").arg(0).arg(n), 3000);
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
    int captureCount = m_configDialog ? m_configDialog->getCaptureCount() : 1;
    requestStartCapture(captureCount);
}

void MainWindow::on_actionStop_triggered()
{
    if (!m_appController) {
        return;
    }

    QMetaObject::invokeMethod(m_appController, "stopCapture", Qt::QueuedConnection);
}

void MainWindow::requestStartCapture(int captureCount)
{
    if (!m_appController) {
        return;
    }

    // AppController 活在 m_controllerThread 上，startCapture 只能经队列过去：
    // 直接调用会在 GUI 线程执行，驱动里属于那个线程的采集看门狗 QTimer
    // 会因跨线程启动而失败。
    QMetaObject::invokeMethod(m_appController, "startCapture", Qt::QueuedConnection,
        Q_ARG(int, captureCount));
}

void MainWindow::on_scale_triggered()
{
    if (m_scaleDialog) {
        m_scaleDialog->show();
        m_scaleDialog->raise();
        m_scaleDialog->activateWindow();
    }
}

void MainWindow::on_display_style_triggered()
{
    if (m_displayStyleDialog) {
        m_displayStyleDialog->show();
        m_displayStyleDialog->raise();
        m_displayStyleDialog->activateWindow();
    }
}

void MainWindow::on_showAxes_triggered(bool checked)
{
    if (m_imageViewWidget) {
        m_imageViewWidget->setAxesVisible(checked);
    }
}

void MainWindow::restoreTheme()
{
    QSettings settings;
    const QString value = settings.value("ui/themeMode").toString();
    if (value == QStringLiteral("dark")) {
        Theme::instance()->setMode(Theme::Mode::Dark);
    } else if (value == QStringLiteral("light")) {
        Theme::instance()->setMode(Theme::Mode::Light);
    }
    // 没有记录就是「跟随系统」，main() 里已经按系统设过了，这里不用动
}

void MainWindow::setThemeMode(Theme::Mode mode)
{
    // 用户选了 Light/Dark 就存下来，下次启动直接用；选 System 时把这个键删掉，
    // 好让「跟随系统」恢复成默认值而不是记住上一次的强制选择
    QSettings settings;
    if (mode == Theme::Mode::System) {
        settings.remove("ui/themeMode");
    } else {
        settings.setValue("ui/themeMode", mode == Theme::Mode::Dark ? "dark" : "light");
    }

    Theme::instance()->setMode(mode);
    syncThemeMenu();
}

void MainWindow::syncThemeMenu()
{
    // Theme 可能被 --theme 改过（比如命令行指定了 dark），菜单要跟着显示实际选项
    switch (Theme::instance()->mode()) {
        case Theme::Mode::Light:
            ui->menuActionThemeLight->setChecked(true);
            break;
        case Theme::Mode::Dark:
            ui->menuActionThemeDark->setChecked(true);
            break;
        case Theme::Mode::System:
            ui->menuActionThemeSystem->setChecked(true);
            break;
    }
}

void MainWindow::on_fillWindow_triggered(bool checked)
{
    if (m_imageViewWidget) {
        m_imageViewWidget->setFitMode(
            checked ? ImageViewWidget::FitMode::FillWindow
                    : ImageViewWidget::FitMode::KeepAspectRatio);
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
    dialog->setModal(false);
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

    // Row range 始终基于 original 2D 图的高度（若存在），而不是当前显示的视图
    // (spectrumView 时 image.height()==1，否则对话框 spinbox 会被锁死在 [1,1])。
    const int imageHeight = m_currentFrame.hasOriginal()
        ? m_currentFrame.originalImage.height()
        : m_currentFrame.image.height();
    if (imageHeight <= 0) {
        QMessageBox::warning(this, tr("Invalid Frame"),
            tr("Cannot set row range: current image has no rows."));
        return;
    }

    RowRangeDialog *dialog = new RowRangeDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setImageHeight(imageHeight);
    dialog->setRange(m_vBinStartRow - 1, (m_vBinEndRow < 0 ? imageHeight : m_vBinEndRow) - 1);
    dialog->show();

    connect(dialog, &RowRangeDialog::applyClicked, this, [this](int startRow, int endRow) {
        m_vBinStartRow = startRow;
        m_vBinEndRow = endRow;

        if (m_currentFrame.isValid()) {
            ImageData frame = m_currentFrame;
            if (frame.hasOriginal()) {
                frame.image = frame.originalImage;
            }
            PostProcess::verticalBinning(frame, m_vBinStartRow, m_vBinEndRow);
            m_currentFrame = frame;
            m_vBinEnabled = true;
            ui->menuActionVerticalBinning->blockSignals(true);
            ui->menuActionVerticalBinning->setChecked(true);
            ui->menuActionVerticalBinning->blockSignals(false);
            updateDisplay(frame);
        }
    });
}

void MainWindow::onCameraStateChanged(CameraState newState)
{
    QString stateText;
    switch (newState) {
    case CameraState::Connecting:
        stateText = tr("Connecting");
        m_fpsTimer->stop();
        m_fpsFrameCount = 0;
        m_fpsValue = 0;
        updateFpsDisplay();

        // disable capture related shortcut
        shortcutLive->setEnabled(false);
        shortcutSingle->setEnabled(false);
        shortcutBurst->setEnabled(false);
        break;
    case CameraState::Connected:
        stateText = tr("Connected");

        // enable capture related shortcut
        shortcutLive->setEnabled(true);
        shortcutSingle->setEnabled(true);
        shortcutBurst->setEnabled(true);
        break;
    case CameraState::Acquiring:
        stateText = tr("Acquiring");
        m_fpsFrameCount = 0;
        m_fpsValue = 0;
        m_fpsTimer->start(1000);
        updateFpsDisplay();
        
        // disable capture related shortcut
        shortcutLive->setEnabled(false);
        shortcutSingle->setEnabled(false);
        shortcutBurst->setEnabled(false);
        break;
    case CameraState::Error:
        stateText = tr("Error");
        if (m_acquiringDark) {
            cancelDarkAcquisition(tr("Camera entered error state."));
        }
        m_fpsTimer->stop();

        // disable capture related shortcut
        shortcutLive->setEnabled(false);
        shortcutSingle->setEnabled(false);
        shortcutBurst->setEnabled(false);
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

    // 1. Dark-frame acquisition: build the running average across N frames.
    //    During a burst, calibration is suppressed (we are building the dark,
    //    not subtracting it).
    if (m_acquiringDark) {
        const QImage &img = m_currentFrame.image;
        if (!img.isNull()) {
            const int width = img.width();
            const int height = img.height();
            const QImage::Format fmt = img.format();

            if (!m_darkAccumInit) {
                m_darkAccumFormat = fmt;
                int elementCount = width * height;
                if (fmt == QImage::Format_RGB888) {
                    elementCount *= 3;
                }
                // QList::fill is fill(value, size) — value first.
                m_darkAccumSum.fill(0u, static_cast<qsizetype>(elementCount));
                m_darkAccumInit = true;
            }

            if (width > 0 && height > 0 && fmt == m_darkAccumFormat) {
                if (fmt == QImage::Format_Grayscale16) {
                    const ushort *srcData = reinterpret_cast<const ushort *>(img.constBits());
                    #pragma omp parallel for schedule(static)
                    for (int y = 0; y < height; ++y) {
                        const int rowBase = y * width;
                        for (int x = 0; x < width; ++x) {
                            m_darkAccumSum[rowBase + x] += srcData[rowBase + x];
                        }
                    }
                } else if (fmt == QImage::Format_Grayscale8) {
                    const uchar *srcData = img.constBits();
                    #pragma omp parallel for schedule(static)
                    for (int y = 0; y < height; ++y) {
                        const int rowBase = y * width;
                        for (int x = 0; x < width; ++x) {
                            m_darkAccumSum[rowBase + x] += srcData[rowBase + x];
                        }
                    }
                } else if (fmt == QImage::Format_RGB888) {
                    const uchar *srcData = img.constBits();
                    const int rowStride = width * 3;
                    #pragma omp parallel for schedule(static)
                    for (int y = 0; y < height; ++y) {
                        const int rowBase = y * rowStride;
                        for (int x = 0; x < rowStride; ++x) {
                            m_darkAccumSum[rowBase + x] += srcData[rowBase + x];
                        }
                    }
                }
            }

            ++m_darkAccumFrames;
            --m_darkBurstRemaining;

            // Compute running average so far (rounded half-up)
            const quint64 divisor = m_darkAccumFrames;
            const quint64 halfDiv = divisor / 2;
            QImage result(width, height, fmt);
            if (fmt == QImage::Format_Grayscale16) {
                ushort *dstData = reinterpret_cast<ushort *>(result.bits());
                for (int i = 0; i < width * height; ++i) {
                    quint64 v = (m_darkAccumSum[i] + halfDiv) / divisor;
                    if (v > 65535) v = 65535;
                    dstData[i] = static_cast<ushort>(v);
                }
            } else if (fmt == QImage::Format_Grayscale8) {
                uchar *dstData = result.bits();
                for (int i = 0; i < width * height; ++i) {
                    quint64 v = (m_darkAccumSum[i] + halfDiv) / divisor;
                    if (v > 255) v = 255;
                    dstData[i] = static_cast<uchar>(v);
                }
            } else if (fmt == QImage::Format_RGB888) {
                uchar *dstData = result.bits();
                const int n = width * height * 3;
                for (int i = 0; i < n; ++i) {
                    quint64 v = (m_darkAccumSum[i] + halfDiv) / divisor;
                    if (v > 255) v = 255;
                    dstData[i] = static_cast<uchar>(v);
                }
            }

            m_currentFrame.image = result;

            showStatusMessage(tr("Acquiring dark frame (%1/%2)...")
                .arg(m_darkBurstTotal - m_darkBurstRemaining)
                .arg(m_darkBurstTotal), 2000);

            if (m_darkBurstRemaining <= 0) {
                m_darkFrame = result;
                m_darkFrameValid = true;
                m_acquiringDark = false;
                // 窗口开着就把新采到的暗帧显示出来，用户当场能勾 Enable
                refreshCalibrationDialog();
                showStatusMessage(
                    tr("Dark frame acquired (%1-frame average). Use Save Frame to keep it.")
                    .arg(m_darkBurstTotal), 10000);
            }
        }

        updateDisplay(m_currentFrame);
        updateToolbarState();

        QSettings settings;
        if (settings.value("data/autoSaveEnabled", false).toBool()) {
            on_actionSaveFrame_triggered();
        }
        return;
    }

    // 2. Calibration branch (only after the burst has completed)
    if (m_darkEnabled && m_darkFrameValid) {
        if (m_darkFrame.size() != m_currentFrame.image.size()) {
            const quint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (nowMs - m_lastDarkSizeWarnMs >= 1000) {
                m_lastDarkSizeWarnMs = nowMs;
                showStatusMessage(
                    tr("Dark-frame calibration skipped: size mismatch "
                       "(dark %1x%2 vs frame %3x%4).")
                        .arg(m_darkFrame.width()).arg(m_darkFrame.height())
                        .arg(m_currentFrame.image.width())
                        .arg(m_currentFrame.image.height()),
                    2000);
            }
        } else {
            PostProcess::applyDarkCalibration(m_currentFrame, &m_darkFrame, m_darkBias);
        }
    }

    // 3. vbin (pre-calibration processing, applies to display only)
    if (m_vBinEnabled) {
        ImageData processed = m_currentFrame;
        PostProcess::verticalBinning(processed, m_vBinStartRow, m_vBinEndRow);
        m_currentFrame = processed;
    }

    updateDisplay(m_currentFrame);
    updateToolbarState();

    QSettings settings2;
    if (settings2.value("data/autoSaveEnabled", false).toBool()) {
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
    // If a dark-frame burst was in flight, the user aborted via Stop (or
    // the driver stopped early). Discard the partial accumulation; the
    // existing m_darkFrame is left untouched so a previously-acquired dark
    // frame stays valid.
    if (m_acquiringDark) {
        cancelDarkAcquisition(tr("Capture stopped before burst completed."));
    }
}

void MainWindow::cancelDarkAcquisition(const QString &reason)
{
    if (!m_acquiringDark) {
        return;
    }
    m_acquiringDark = false;
    m_darkBurstRemaining = 0;
    m_darkAccumInit = false;
    m_darkAccumFrames = 0;
    m_darkAccumSum.clear();
    if (m_calibrationDialog) {
        m_calibrationDialog->setAcquireInProgress(false);
    }
    showStatusMessage(QString("Dark-frame acquisition cancelled: %1").arg(reason), 5000);
}

void MainWindow::onCrosshairCleared()
{
    ui->setCrosshairInfo(QString());
}

void MainWindow::onCrosshairMoved(const QPointF &position, int value)
{
    ui->setCrosshairInfo(QString("Crosshair: X: %1, Y: %2, Value: %3")
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

void MainWindow::on_histogram_triggered()
{
    if (!m_histogramWindow) {
        m_histogramWindow = new HistogramWindow(this);
    }

    if (m_imageViewWidget->hasImage()) {
        m_histogramWindow->setImage(m_imageViewWidget->image());
    }

    m_histogramWindow->show();
}

void MainWindow::onLiveModeTriggered()
{
    if (m_appController) {
        requestStartCapture(0);
        showStatusMessage("Mode: Live", 3000);
    }
}

void MainWindow::onSingleModeTriggered()
{
    if (m_appController) {
        requestStartCapture(1);
        showStatusMessage("Mode: Single", 3000);
    }
}

void MainWindow::onBurstModeTriggered()
{
    if (m_appController) {
        requestStartCapture(5);
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
        ui->menuActionCalibration->setEnabled(false);
    }

    const bool connected = m_appController->isConnected();
    const bool acquiring = m_appController->state() == CameraState::Acquiring;

    ui->actionStart->setEnabled(connected && !acquiring);
    ui->actionStop->setEnabled(acquiring);
    ui->menuActionCalibration->setEnabled(connected);

    if (m_calibrationDialog) {
        m_calibrationDialog->setAcquireEnabled(connected && !acquiring && !m_acquiringDark);
    }
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
        if (!frame.spectrum.isEmpty()) {
            m_spectrumViewWidget->setSpectrumData(frame.spectrum);
        } else {
            m_spectrumViewWidget->setFromImage(frame.image);
        }
    } else {
        m_imageViewWidget->setImage(frame.image);

        if (m_profileWindow && m_profileWindow->isVisible()
            && m_imageViewWidget->crosshairCount() > 0) {
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

        if (m_histogramWindow && m_histogramWindow->isVisible()) {
            m_histogramWindow->setImage(frame.image);
        }
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
    } else {
        ui->centralStackedWidget->setCurrentWidget(m_imageViewWidget);
        QCoreApplication::processEvents();
    }

    // 直方图统计的是图像，spectrumView 下没有可统计的对象，所以只在
    // imageView 模式下放开菜单项。只在模式真的变了时才动，switchView
    // 是每帧都调的，没必要每帧都去碰一遍 QAction
    const bool imageViewActive = (height != 1);
    if (imageViewActive != m_imageViewActive) {
        m_imageViewActive = imageViewActive;
        ui->menuActionHistogram->setEnabled(imageViewActive);
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

void MainWindow::onFileSaveFailed(const QString &error, const QString &/*details*/)
{
    QMessageBox::critical(this, tr("Save Error"),
        tr("Failed to save file:\n%1").arg(error));
}