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
#include "dialogs/ScaleControlDialog.h"
#include "dialogs/DisplayStyleDialog.h"
#include "config/CameraConfigDialog.h"
#include "PostProcess.h"
#include "../workers/FileSaverWorker.h"
#include "../workers/FileLoaderWorker.h"
#include "../workers/SaveTypes.h"
#include "../workers/formats/CsvFormatHandler.h"
#include "../workers/formats/TiffFormatHandler.h"

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

    m_scaleDialog = new ScaleControlDialog(this);
    m_scaleDialog->setImageScaleType(0);
    m_scaleDialog->setImageColorScaleMode(0);
    m_scaleDialog->setSpectrumScaleType(0);

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
    connect(m_imageViewWidget, &ImageViewWidget::overexposureDetected,
            this, &MainWindow::onOverexposureDetected);

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

    connect(ui->menuActionFillWindow, &QAction::toggled,
            this, &MainWindow::on_fillWindow_triggered);

    connect(ui->menuActionStatistics, &QAction::triggered,
            this, &MainWindow::on_statistics_triggered);

    connect(ui->menuActionVerticalBinning, &QAction::toggled,
            this, &MainWindow::on_verticalBinning_triggered);
    connect(ui->menuActionRowRange, &QAction::triggered,
            this, &MainWindow::on_rowRange_triggered);

    connect(ui->menuActionProfile, &QAction::triggered,
            this, &MainWindow::on_profile_triggered);

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

void MainWindow::onOverexposureDetected(QPoint position)
{
    ui->overexposureLabel->setText(QString("Overexposed: X: %1, Y: %2").arg(position.x()).arg(position.y()));
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