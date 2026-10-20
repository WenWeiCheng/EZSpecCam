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
#include "dialogs/DespikeDialog.h"
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

#include <algorithm>
#include <cmath>

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
    // 和旧实现保持同一个默认输出偏置，用户不必每次重新输入
    m_darkSettings.outputOffset = 1000;

    // 必须在 setupUi 之前：绘图控件是在那里创建的，创建时就会按当前主题取色
    restoreTheme();

    ui->setupUi(this);

    // 状态栏那一格的内容以后统一由 updateStateLabel() 拼，初始值也从这里给，
    // 不再让 MainWindowUi 里写死的 "State: Disconnected" 和实际格式对不上
    m_cameraStateText = tr("Disconnected");
    updateStateLabel();

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

    connect(ui->menuActionDespike, &QAction::toggled,
            this, &MainWindow::on_despike_toggled);
    connect(ui->menuActionDespikeSettings, &QAction::triggered,
            this, &MainWindow::on_despikeSettings_triggered);

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
    // vBinStartRow / vBinEndRow 是 1-based 闭区间，和 verticalBinning 的解释一致。
    // 行范围是相对原始 2D 图的：enabled 时 image 已经是 1 行，得看 originalImage
    const int h = request.frame.hasOriginal()
        ? request.frame.originalImage.height()
        : request.frame.image.height();
    if (h > 0) {
        const int effEnd = (m_vBinEndRow < 0) ? h : m_vBinEndRow;
        const bool meaningfulRange = m_vBinEnabled
            && !(m_vBinStartRow <= 1 && effEnd >= h);
        if (meaningfulRange) {
            request.frame.softwareSettings["softwareVerticalBinning"] = true;
            request.frame.softwareSettings["vBinStartRow"] = m_vBinStartRow;
            request.frame.softwareSettings["vBinEndRow"] = effEnd;
        } else {
            request.frame.softwareSettings["softwareVerticalBinning"] = false;
        }

        // 保存的 m_currentFrame 已经是去尖峰（+ 可选 vbin）之后的帧，这里只把
        // 参数写进 metadata 供以后追溯；参考帧不做去尖峰，也不能声称处理过
        const bool despikeApplied = ui->menuActionDespike->isChecked()
            && !m_currentFrameIsReference
            && m_lastDespikeResult.sampleCount > 0;
        request.frame.softwareSettings["despikeApplied"] = despikeApplied;
        if (despikeApplied) {
            int startRow = 1;
            int endRow = -1;
            effectiveDespikeRows(h, &startRow, &endRow);
            request.frame.softwareSettings["despikePrimaryThreshold"] =
                m_despikeParams.primaryThreshold;
            request.frame.softwareSettings["despikeStartRow"] = startRow;
            request.frame.softwareSettings["despikeEndRow"] = (endRow < 0) ? h : endRow;
        }

        // vbin + 部分行范围时，实时管线只处理了 ROI，originalImage 还是原始
        // 二维图；保存的必须包含去尖峰结果，所以在这里按同一行范围补做一次。
        // 全范围 / 未开 vbin 的情况下 originalImage 已经是处理后的（或为空）
        if (ui->menuActionDespike->isChecked() && !m_currentFrameIsReference
            && request.frame.hasOriginal() && !m_originalImageIsDespiked) {
            int startRow = 1;
            int endRow = -1;
            effectiveDespikeRows(h, &startRow, &endRow);
            PostProcess::ImageDespikeResult saveResult;
            request.frame.originalImage =
                PostProcess::despikeImage(request.frame.originalImage, m_despikeParams,
                                          &saveResult, startRow - 1,
                                          endRow < 0 ? -1 : endRow - 1);
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
    if (m_loadingReference) {
        m_loadingReference = false;

        Calibration::ReferenceFrame reference;
        reference.exposure = m_loadingReferenceExposure;
        reference.frameCount = 1;
        reference.source = m_loadingReferencePath;
        QString planeError;
        reference.plane = Calibration::planeFromImage(result.frame.image, &planeError);
        if (reference.isNull()) {
            showStatusMessage(tr("Failed to load calibration reference: %1").arg(planeError),
                              5000);
            return;
        }

        storeReferenceFrame(m_loadingReferenceKind, reference);
        refreshCalibrationDialog();
        showStatusMessage(tr("Calibration reference loaded: %1")
                              .arg(QFileInfo(m_loadingReferencePath).fileName()),
                          5000);
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

    // Loaded frames go through the same calibration pipeline as live frames.
    // The manual signal exposure in the dialog is what the dark scaling uses.
    applyCalibration(m_currentFrame);
    m_currentFrameIsReference = false;
    m_preDespikeFrame = m_currentFrame;
    m_preDespikeFrame.originalImage = QImage();

    const QImage &img = m_currentFrame.image;
    const int imgHeight = img.height();
    const QVariantMap &sw = m_currentFrame.softwareSettings;

    // 自动恢复 spectrum：仅当 metadata 含 vbin 范围，且该范围是图像行数的真子集时。
    // metadata 里的 vBinStartRow / vBinEndRow 是 1-based 闭区间
    const int savedStart = sw.value("vBinStartRow", -1).toInt();
    const int savedEnd = sw.value("vBinEndRow", -1).toInt();
    const bool hasRange = (savedStart >= 0) && (savedEnd >= 0);
    const bool fullRange = hasRange && (savedStart <= 1) && (savedEnd >= imgHeight);

    const bool shouldRecover = result.hasMetadata
        && hasRange
        && !fullRange
        && imgHeight > 1;

    // 去尖峰和 vbin 共用行范围。vbin 关闭时 metadata 里没有 vBinStartRow，
    // 但可能带着去尖峰实际用过的范围，加载后要沿用，否则再开去尖峰会去处理
    // 当初刻意保留的范围外数据
    const bool savedDespikeApplied = sw.value("despikeApplied", false).toBool();
    const int savedDespikeStart = sw.value("despikeStartRow", -1).toInt();
    const int savedDespikeEnd = sw.value("despikeEndRow", -1).toInt();
    const bool hasDespikeRange = savedDespikeApplied
        && savedDespikeStart >= 1
        && savedDespikeEnd >= savedDespikeStart;

    if (shouldRecover) {
        m_vBinStartRow = qBound(1, savedStart, imgHeight);
        m_vBinEndRow = qBound(m_vBinStartRow, savedEnd, imgHeight);
        m_vBinEnabled = true;
    } else {
        // 缺 metadata / 无 vbin 范围 / 范围覆盖整图 → 直接显示 2D 图
        m_vBinEnabled = false;
        if (hasDespikeRange && imgHeight > 0) {
            m_vBinStartRow = qBound(1, savedDespikeStart, imgHeight);
            m_vBinEndRow = qBound(m_vBinStartRow, savedDespikeEnd, imgHeight);
        } else {
            m_vBinStartRow = 1;
            m_vBinEndRow = (imgHeight > 0) ? imgHeight : -1;
        }
    }

    ui->menuActionVerticalBinning->blockSignals(true);
    ui->menuActionVerticalBinning->setChecked(m_vBinEnabled);
    ui->menuActionVerticalBinning->blockSignals(false);

    // 去尖峰和 vbin 都从校正后的原图重新算，行范围沿用 metadata 里恢复出来的值
    rebuildDespikeAndBinning(true);

    QString msg = tr("Loaded %1").arg(QFileInfo(filePath).fileName());
    QStringList parts;
    if (result.hasMetadata) parts << tr("with metadata");
    if (shouldRecover) parts << tr("spectrum recovered");
    if (!parts.isEmpty()) msg += QStringLiteral(" (") + parts.join(QStringLiteral(", ")) + QStringLiteral(")");
    showStatusMessage(msg, 3000);
}

void MainWindow::onFrameLoadFailed(const QString &error, const QString &filePath)
{
    if (m_loadingReference) {
        m_loadingReference = false;
        showStatusMessage(tr("Failed to load calibration reference %1: %2")
                              .arg(QFileInfo(filePath).fileName(), error),
                          5000);
        return;
    }
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
    // 状态栏上的 Mode 要一直跟着对话框里那个下拉框走：用户在对话框里改了，
    // 不用再按一次快捷键
    connect(m_configDialog, &CameraConfigDialog::captureModeChanged,
            this, [this](const QString &mode) {
                if (mode == QStringLiteral("Live")) {
                    m_captureMode = CaptureMode::Live;
                } else if (mode == QStringLiteral("Single")) {
                    m_captureMode = CaptureMode::Single;
                } else if (mode == QStringLiteral("Burst")) {
                    m_captureMode = CaptureMode::Burst;
                }
                updateStateLabel();
            });
    m_configDialog->show();
}
void MainWindow::on_actionCalibration_triggered()
{
    if (!m_calibrationDialog) {
        m_calibrationDialog = new CalibrationDialog(this);
        connect(m_calibrationDialog, &CalibrationDialog::applied,
                this, &MainWindow::onCalibrationApplied);
        connect(m_calibrationDialog, &CalibrationDialog::acquireRequested,
                this, [this](int frameCount) {
                    if (!m_calibrationDialog) {
                        return;
                    }
                    const auto kind = static_cast<Calibration::ReferenceKind>(
                        m_calibrationDialog->acquisitionKind());
                    startReferenceAcquisition(kind, frameCount,
                                              m_calibrationDialog->acquisitionExposure());
                });
        connect(m_calibrationDialog, &CalibrationDialog::referenceFileSelected,
                this, &MainWindow::onReferenceFileSelected);
        connect(m_calibrationDialog, &CalibrationDialog::vffRemoveRequested,
                this, &MainWindow::onVffRemoveRequested);
        connect(m_calibrationDialog, &CalibrationDialog::vffClearRequested,
                this, &MainWindow::onVffClearRequested);
    }

    refreshCalibrationDialog(true);
    m_calibrationDialog->show();
    m_calibrationDialog->raise();
    m_calibrationDialog->activateWindow();
}

// 窗口开着的时候状态也得跟着变：采完参考帧当场把「已经采到 N 帧平均」显示出来，
// 用户不用关掉再打开就能勾 Enable。
void MainWindow::refreshCalibrationDialog(bool reloadConfig)
{
    if (!m_calibrationDialog) {
        return;
    }

    // 重新打开窗口时把已应用的配置读回控件；采完参考帧后的刷新只更新状态，
    // 不能把用户正在输入的方法、曝光时间覆盖成上一次 OK 时的值。
    if (reloadConfig) {
        m_calibrationDialog->setDarkFrameEnabled(m_darkSettings.enabled);
        m_calibrationDialog->setDarkMethod(m_darkSettings.method);
        m_calibrationDialog->setMatchedDarkExposure(m_darkSettings.matchedExposure);
        m_calibrationDialog->setLongDarkExposure(m_darkSettings.longDarkExposure);
        m_calibrationDialog->setTargetExposure(m_darkSettings.targetExposure);
        m_calibrationDialog->setCustomBias(m_darkSettings.outputOffset);

        m_calibrationDialog->setFlatEnabled(m_flatSettings.enabled);
        m_calibrationDialog->setFlatMethod(m_flatSettings.method);
        m_calibrationDialog->setFlatExposure(m_flatSettings.flatExposure);
        m_calibrationDialog->setVffExposure(m_flatSettings.vffEntries.isEmpty()
                                                ? 0.0
                                                : m_flatSettings.vffEntries.last().exposure);
    }

    const auto setStatus = [this](Calibration::ReferenceKind kind,
                                  const Calibration::ReferenceFrame &reference) {
        m_calibrationDialog->setReferenceStatus(kind, reference.isValid(),
                                                reference.isInMemory(),
                                                reference.frameCount, reference.source);
    };
    setStatus(Calibration::ReferenceKind::MatchedDark, m_darkSettings.matchedDark);
    setStatus(Calibration::ReferenceKind::Bias, m_darkSettings.bias);
    setStatus(Calibration::ReferenceKind::LongDark, m_darkSettings.longDark);
    setStatus(Calibration::ReferenceKind::Flat, m_flatSettings.flat);
    m_calibrationDialog->setVffEntries(m_flatSettings.vffEntries);

    if (!m_flatSettings.enabled) {
        m_calibrationDialog->setFlatFieldStatus(tr("Flat-field calibration disabled."), true);
    } else if (m_flatFieldValid) {
        const int samples = m_flatField.width * m_flatField.height * m_flatField.channels;
        const double excludedPct = samples > 0
            ? 100.0 * m_flatField.excludedSamples / samples
            : 0.0;
        const bool vff = m_flatSettings.method == Calibration::FlatMethod::VariableExposure;
        m_calibrationDialog->setFlatFieldStatus(
            vff
                ? tr("Ready: mean slope %1 ADU/s, gain %2..%3, %4% of samples left uncorrected.")
                      .arg(m_flatField.meanSignal, 0, 'f', 1)
                      .arg(m_flatField.minGain, 0, 'f', 4)
                      .arg(m_flatField.maxGain, 0, 'f', 4)
                      .arg(excludedPct, 0, 'f', 2)
                : tr("Ready: mean signal %1 ADU, gain %2..%3, %4% of samples left uncorrected.")
                      .arg(m_flatField.meanSignal, 0, 'f', 1)
                      .arg(m_flatField.minGain, 0, 'f', 4)
                      .arg(m_flatField.maxGain, 0, 'f', 4)
                      .arg(excludedPct, 0, 'f', 2),
            true);
    } else {
        m_calibrationDialog->setFlatFieldStatus(
            m_flatFieldError.isEmpty()
                ? tr("Not ready: add the required flat reference frame(s) and press OK.")
                : tr("Not ready: %1").arg(m_flatFieldError),
            false);
    }

    m_calibrationDialog->setFrameCount(m_referenceBurstTotal);
    m_calibrationDialog->setAcquireInProgress(m_acquiringReference);

    const bool connected = m_appController && m_appController->isConnected();
    const bool capturing = connected && m_appController->state() == CameraState::Acquiring;
    m_calibrationDialog->setAcquireEnabled(connected && !capturing && !m_acquiringReference);
}

void MainWindow::onCalibrationApplied(bool enabled, const QString &path, int bias)
{
    Q_UNUSED(path);
    if (!m_calibrationDialog) {
        return;
    }

    m_darkSettings.enabled = enabled;
    m_darkSettings.method = m_calibrationDialog->darkMethod();
    m_darkSettings.matchedExposure = m_calibrationDialog->matchedDarkExposure();
    m_darkSettings.longDarkExposure = m_calibrationDialog->longDarkExposure();
    m_darkSettings.targetExposure = m_calibrationDialog->targetExposure();
    m_darkSettings.outputOffset = bias;

    m_flatSettings.enabled = m_calibrationDialog->flatEnabled();
    m_flatSettings.method = m_calibrationDialog->flatMethod();
    m_flatSettings.flatExposure = m_calibrationDialog->flatExposure();

    rebuildFlatField(true);

    // 兼容旧路径：若只给了路径还没真正读过文件，这里补一次加载。
    if (enabled
        && m_darkSettings.method == Calibration::DarkMethod::MatchedDark
        && m_darkSettings.matchedDark.isNull()
        && !m_calibrationDialog->darkFramePath().isEmpty()) {
        onReferenceFileSelected(static_cast<int>(Calibration::ReferenceKind::MatchedDark),
                                m_calibrationDialog->darkFramePath());
    }

    QStringList parts;
    if (m_darkSettings.enabled) {
        parts << (m_darkSettings.method == Calibration::DarkMethod::MatchedDark
                      ? tr("dark (matched)")
                      : tr("dark (bias + scaled)"));
    }
    if (m_flatSettings.enabled) {
        parts << (m_flatSettings.method == Calibration::FlatMethod::Classic
                      ? tr("flat (classic)")
                      : tr("flat (variable exposure)"));
    }
    showStatusMessage(parts.isEmpty()
                          ? tr("Calibration disabled.")
                          : tr("Calibration enabled: %1.").arg(parts.join(QStringLiteral(", "))),
                      3000);

    // The Apply button leaves the dialog open, so refresh its flat-field
    // readiness text immediately. Scalar fields stay as the user typed them.
    refreshCalibrationDialog(false);
}

void MainWindow::onAcquireDarkFrameStartRequested(int frameCount)
{
    const double exposure = m_calibrationDialog
        ? m_calibrationDialog->matchedDarkExposure()
        : m_darkSettings.matchedExposure;
    startReferenceAcquisition(Calibration::ReferenceKind::MatchedDark, frameCount, exposure);
}

QString MainWindow::referenceName(Calibration::ReferenceKind kind) const
{
    switch (kind) {
    case Calibration::ReferenceKind::MatchedDark:
        return tr("dark frame");
    case Calibration::ReferenceKind::Bias:
        return tr("bias frame");
    case Calibration::ReferenceKind::LongDark:
        return tr("long dark frame");
    case Calibration::ReferenceKind::Flat:
        return tr("flat frame");
    case Calibration::ReferenceKind::VffEntry:
        return tr("variable-exposure flat frame");
    }
    return tr("reference frame");
}

void MainWindow::startReferenceAcquisition(Calibration::ReferenceKind kind,
                                           int frameCount, double exposure)
{
    if (m_acquiringReference) {
        showStatusMessage(tr("Reference acquisition already in progress."), 2000);
        return;
    }
    // 对话框的输入范围就是 CalibrationDialog::kMaxFrameCount，这里再夹一次是
    // 防止别的调用方（测试、快捷键）绕过它。真夹到了说明上游放进了超范围的数。
    const int n = qBound(1, frameCount, CalibrationDialog::kMaxFrameCount);

    m_acquiringKind = kind;
    m_acquiringExposure = exposure;
    m_referenceBurstTotal = n;
    m_referenceBurstRemaining = n;
    m_referenceAccumInit = false;
    m_referenceAccumFrames = 0;
    m_referenceAccumWidth = 0;
    m_referenceAccumHeight = 0;
    m_referenceAccumFormat = QImage::Format_Invalid;
    m_referenceAccumSum.clear();

    QMetaObject::invokeMethod(m_appController, "startCapture",
        Qt::QueuedConnection, Q_ARG(int, n));

    m_acquiringReference = true;
    if (m_calibrationDialog) {
        m_calibrationDialog->setAcquireInProgress(true);
    }

    showStatusMessage(tr("Acquiring %1 (%2/%3)...")
                          .arg(referenceName(kind)).arg(0).arg(n), 3000);
}

void MainWindow::onReferenceFileSelected(int kind, const QString &path)
{
    if (m_loadingReference) {
        showStatusMessage(tr("A calibration reference is already loading."), 2000);
        return;
    }

    m_loadingReference = true;
    m_loadingReferenceKind = static_cast<Calibration::ReferenceKind>(kind);
    m_loadingReferenceExposure = 0.0;
    if (m_calibrationDialog) {
        switch (m_loadingReferenceKind) {
        case Calibration::ReferenceKind::MatchedDark:
            m_loadingReferenceExposure = m_calibrationDialog->matchedDarkExposure();
            break;
        case Calibration::ReferenceKind::Bias:
            // The bias frame is the zero-exposure reference; no time stamp is
            // needed by the calibration model.
            m_loadingReferenceExposure = 0.0;
            break;
        case Calibration::ReferenceKind::LongDark:
            m_loadingReferenceExposure = m_calibrationDialog->longDarkExposure();
            break;
        case Calibration::ReferenceKind::Flat:
            m_loadingReferenceExposure = m_calibrationDialog->flatExposure();
            break;
        case Calibration::ReferenceKind::VffEntry:
            m_loadingReferenceExposure = m_calibrationDialog->vffExposure();
            break;
        }
    }
    m_loadingReferencePath = path;

    QMetaObject::invokeMethod(m_fileLoaderWorker, "loadFrame",
        Qt::QueuedConnection, Q_ARG(QString, path));
    showStatusMessage(tr("Loading %1: %2")
                          .arg(referenceName(m_loadingReferenceKind),
                               QFileInfo(path).fileName()), 3000);
}

void MainWindow::onVffRemoveRequested(int index)
{
    if (index < 0 || index >= m_flatSettings.vffEntries.size()) {
        return;
    }
    // The list is displayed sorted by exposure; remove the matching element.
    QVector<Calibration::ReferenceFrame> sorted = m_flatSettings.vffEntries;
    std::sort(sorted.begin(), sorted.end(), [](const Calibration::ReferenceFrame &a,
                                                const Calibration::ReferenceFrame &b) {
        return a.exposure < b.exposure;
    });
    if (index >= sorted.size()) {
        return;
    }
    const Calibration::ReferenceFrame removed = sorted.at(index);
    for (int i = 0; i < m_flatSettings.vffEntries.size(); ++i) {
        const Calibration::ReferenceFrame &entry = m_flatSettings.vffEntries.at(i);
        if (entry.exposure == removed.exposure && entry.source == removed.source
            && entry.frameCount == removed.frameCount) {
            m_flatSettings.vffEntries.removeAt(i);
            break;
        }
    }
    if (m_flatSettings.enabled) {
        rebuildFlatField(false);
    }
    refreshCalibrationDialog();
}

void MainWindow::onVffClearRequested()
{
    m_flatSettings.vffEntries.clear();
    if (m_flatSettings.enabled) {
        rebuildFlatField(false);
    }
    refreshCalibrationDialog();
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

    // vbin 的行范围同时也决定去尖峰的范围，所以两者一起重建
    rebuildDespikeAndBinning(true);
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
    // 对话框是 0-based（和 Profile 行号一致），成员/meta 是 1-based
    dialog->setRange(RowRangeDialog::fromBinningRow(m_vBinStartRow),
                     RowRangeDialog::fromBinningRow(
                         m_vBinEndRow < 0 ? imageHeight : m_vBinEndRow));
    dialog->show();

    connect(dialog, &RowRangeDialog::applyClicked, this, [this](int startRow, int endRow) {
        // 对话框给的是 0-based 行索引，转成 verticalBinning/metadata 用的 1-based
        m_vBinStartRow = RowRangeDialog::toBinningRow(startRow);
        m_vBinEndRow = RowRangeDialog::toBinningRow(endRow);

        m_vBinEnabled = true;
        ui->menuActionVerticalBinning->blockSignals(true);
        ui->menuActionVerticalBinning->setChecked(true);
        ui->menuActionVerticalBinning->blockSignals(false);

        // 行范围变了，去尖峰的范围跟着变，从原始帧重算一遍
        rebuildDespikeAndBinning(true);
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
        if (m_acquiringReference) {
            cancelReferenceAcquisition(tr("Camera entered error state."));
        }
        m_fpsTimer->stop();

        // disable capture related shortcut
        shortcutLive->setEnabled(false);
        shortcutSingle->setEnabled(false);
        shortcutBurst->setEnabled(false);
        break;
    }
    m_cameraStateText = stateText;
    updateStateLabel();

    updateToolbarState();
}

void MainWindow::onCameraFrameReady(const ImageData &frame)
{
    m_currentFrame = frame;

    ui->frameCountLabel->setText(tr("Frames: %1").arg(++m_frameCount));
    m_fpsFrameCount++;

    // 1. Reference acquisition: build the running average across N frames.
    //    During a burst calibration is suppressed (we are building a reference,
    //    not correcting a signal frame).
    if (m_acquiringReference) {
        // 参考帧是校准用的原始数据，不参与去尖峰；保存时也不能声称处理过
        m_currentFrameIsReference = true;

        const QImage &img = m_currentFrame.image;
        if (!img.isNull()) {
            const int width = img.width();
            const int height = img.height();
            const QImage::Format fmt = img.format();

            if (width <= 0 || height <= 0
                || (fmt != QImage::Format_Grayscale16
                    && fmt != QImage::Format_Grayscale8
                    && fmt != QImage::Format_RGB888)) {
                cancelReferenceAcquisition(
                    tr("Unsupported frame format for a reference acquisition."));
                return;
            }

            if (!m_referenceAccumInit) {
                m_referenceAccumWidth = width;
                m_referenceAccumHeight = height;
                m_referenceAccumFormat = fmt;
                int elementCount = width * height;
                if (fmt == QImage::Format_RGB888) {
                    elementCount *= 3;
                }
                // QList::fill is fill(value, size) — value first.
                m_referenceAccumSum.fill(0u, static_cast<qsizetype>(elementCount));
                m_referenceAccumInit = true;
            } else if (width != m_referenceAccumWidth
                       || height != m_referenceAccumHeight
                       || fmt != m_referenceAccumFormat) {
                cancelReferenceAcquisition(
                    tr("Frame size or format changed during the reference burst."));
                return;
            }

            // QImage pads every scan line to a 4-byte boundary, so odd widths
            // have bytesPerLine() > width * bytes-per-pixel. Read one row at a
            // time instead of walking constBits() linearly.
            if (fmt == QImage::Format_Grayscale16) {
                #pragma omp parallel for schedule(static)
                for (int y = 0; y < height; ++y) {
                    const ushort *srcRow =
                        reinterpret_cast<const ushort *>(img.constScanLine(y));
                    const int rowBase = y * width;
                    for (int x = 0; x < width; ++x) {
                        m_referenceAccumSum[rowBase + x] += srcRow[x];
                    }
                }
            } else if (fmt == QImage::Format_Grayscale8) {
                #pragma omp parallel for schedule(static)
                for (int y = 0; y < height; ++y) {
                    const uchar *srcRow = img.constScanLine(y);
                    const int rowBase = y * width;
                    for (int x = 0; x < width; ++x) {
                        m_referenceAccumSum[rowBase + x] += srcRow[x];
                    }
                }
            } else { // RGB888
                #pragma omp parallel for schedule(static)
                for (int y = 0; y < height; ++y) {
                    const uchar *srcRow = img.constScanLine(y);
                    const int rowBase = y * width * 3;
                    for (int x = 0; x < width * 3; ++x) {
                        m_referenceAccumSum[rowBase + x] += srcRow[x];
                    }
                }
            }

            ++m_referenceAccumFrames;
            --m_referenceBurstRemaining;

            // Compute the running average (rounded half up)
            const quint64 divisor = m_referenceAccumFrames;
            const quint64 halfDiv = divisor / 2;
            QImage result(width, height, fmt);
            if (fmt == QImage::Format_Grayscale16) {
                for (int y = 0; y < height; ++y) {
                    ushort *dstRow = reinterpret_cast<ushort *>(result.scanLine(y));
                    const int rowBase = y * width;
                    for (int x = 0; x < width; ++x) {
                        quint64 v = (m_referenceAccumSum[rowBase + x] + halfDiv) / divisor;
                        if (v > 65535) v = 65535;
                        dstRow[x] = static_cast<ushort>(v);
                    }
                }
            } else if (fmt == QImage::Format_Grayscale8) {
                for (int y = 0; y < height; ++y) {
                    uchar *dstRow = result.scanLine(y);
                    const int rowBase = y * width;
                    for (int x = 0; x < width; ++x) {
                        quint64 v = (m_referenceAccumSum[rowBase + x] + halfDiv) / divisor;
                        if (v > 255) v = 255;
                        dstRow[x] = static_cast<uchar>(v);
                    }
                }
            } else if (fmt == QImage::Format_RGB888) {
                for (int y = 0; y < height; ++y) {
                    uchar *dstRow = result.scanLine(y);
                    const int rowBase = y * width * 3;
                    for (int x = 0; x < width * 3; ++x) {
                        quint64 v = (m_referenceAccumSum[rowBase + x] + halfDiv) / divisor;
                        if (v > 255) v = 255;
                        dstRow[x] = static_cast<uchar>(v);
                    }
                }
            }

            m_currentFrame.image = result;
            m_currentFrame.originalImage = QImage();
            m_preDespikeFrame = m_currentFrame;

            showStatusMessage(tr("Acquiring %1 (%2/%3)...")
                .arg(referenceName(m_acquiringKind))
                .arg(m_referenceBurstTotal - m_referenceBurstRemaining)
                .arg(m_referenceBurstTotal), 2000);

            if (m_referenceBurstRemaining <= 0) {
                Calibration::ReferenceFrame reference;
                reference.exposure = m_acquiringExposure;
                reference.frameCount = static_cast<int>(m_referenceAccumFrames);
                QString planeError;
                reference.plane = Calibration::planeFromImage(result, &planeError);

                const Calibration::ReferenceKind finishedKind = m_acquiringKind;
                const int frames = m_referenceBurstTotal;
                m_acquiringReference = false;
                m_referenceAccumInit = false;
                m_referenceAccumFrames = 0;
                m_referenceAccumSum.clear();

                if (!reference.isNull()) {
                    storeReferenceFrame(finishedKind, reference);
                    refreshCalibrationDialog();
                    showStatusMessage(
                        tr("%1 acquired (%2-frame average). Use Save Frame to keep it.")
                            .arg(referenceName(finishedKind))
                            .arg(frames), 10000);
                } else {
                    refreshCalibrationDialog();
                    showStatusMessage(
                        tr("Failed to build the %1: %2")
                            .arg(referenceName(finishedKind), planeError), 5000);
                }
            }
        }

        updateDisplay(m_currentFrame);
        updateToolbarState();
        // Do not auto-save the running average: only the completed reference is
        // worth keeping, and the user can press Save Frame explicitly.
        return;
    }

    // 2. Dark subtraction + flat multiplication + output offset
    applyCalibration(m_currentFrame);
    m_currentFrameIsReference = false;

    // 3. 去尖峰 → vbin → 显示。两者都只在 m_preDespikeFrame 上做一次，
    // 所以开关/参数/行范围变化时可以随时从这份校正后的原图重算
    m_preDespikeFrame = m_currentFrame;
    m_preDespikeFrame.originalImage = QImage();
    rebuildDespikeAndBinning();
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
    // If a reference burst was in flight, the user aborted via Stop (or the
    // driver stopped early). Discard the partial accumulation; previously
    // acquired references stay valid.
    if (m_acquiringReference) {
        cancelReferenceAcquisition(tr("Capture stopped before burst completed."));
    }
}

void MainWindow::cancelReferenceAcquisition(const QString &reason)
{
    if (!m_acquiringReference) {
        return;
    }
    m_acquiringReference = false;
    m_referenceBurstRemaining = 0;
    m_referenceAccumInit = false;
    m_referenceAccumFrames = 0;
    m_referenceAccumSum.clear();
    if (m_calibrationDialog) {
        m_calibrationDialog->setAcquireInProgress(false);
    }
    showStatusMessage(tr("%1 acquisition cancelled: %2")
                          .arg(referenceName(m_acquiringKind), reason), 5000);
}

void MainWindow::storeReferenceFrame(Calibration::ReferenceKind kind,
                                     const Calibration::ReferenceFrame &reference)
{
    switch (kind) {
    case Calibration::ReferenceKind::MatchedDark:
        m_darkSettings.matchedDark = reference;
        // The exposure is part of the reference: keep it in sync immediately so
        // flat construction can validate D(t_flat) before the next Apply.
        if (reference.exposure > 0.0) {
            m_darkSettings.matchedExposure = reference.exposure;
        }
        break;
    case Calibration::ReferenceKind::Bias:
        m_darkSettings.bias = reference;
        break;
    case Calibration::ReferenceKind::LongDark:
        m_darkSettings.longDark = reference;
        if (reference.exposure > 0.0) {
            m_darkSettings.longDarkExposure = reference.exposure;
        }
        break;
    case Calibration::ReferenceKind::Flat:
        m_flatSettings.flat = reference;
        if (reference.exposure > 0.0) {
            m_flatSettings.flatExposure = reference.exposure;
        }
        break;
    case Calibration::ReferenceKind::VffEntry: {
        // Re-acquiring the same nominal exposure replaces the entry instead of
        // fitting two nearly identical x values.
        const double tolerance = 1e-6 * std::max(1.0, std::abs(reference.exposure));
        for (int i = 0; i < m_flatSettings.vffEntries.size(); ++i) {
            if (std::abs(m_flatSettings.vffEntries.at(i).exposure - reference.exposure)
                <= tolerance) {
                m_flatSettings.vffEntries[i] = reference;
                break;
            }
        }
        bool replaced = false;
        for (const Calibration::ReferenceFrame &entry : m_flatSettings.vffEntries) {
            if (std::abs(entry.exposure - reference.exposure) <= tolerance) {
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            m_flatSettings.vffEntries.push_back(reference);
        }
        std::sort(m_flatSettings.vffEntries.begin(), m_flatSettings.vffEntries.end(),
                  [](const Calibration::ReferenceFrame &a, const Calibration::ReferenceFrame &b) {
            return a.exposure < b.exposure;
        });
        break;
    }
    }

    // A flat is built on top of the dark model, so a new dark reference can
    // change the gain map just as much as a new flat frame.
    if (m_flatSettings.enabled) {
        rebuildFlatField(false);
    }
}

void MainWindow::rebuildFlatField(bool showWarnings)
{
    m_flatFieldValid = false;
    m_flatField = Calibration::FlatField();
    m_flatFieldError.clear();
    if (!m_flatSettings.enabled) {
        return;
    }

    QString error;
    if (m_flatSettings.method == Calibration::FlatMethod::Classic) {
        m_flatField = Calibration::buildClassicFlat(m_flatSettings, m_darkSettings, &error);
    } else {
        m_flatField = Calibration::buildVariableExposureFlat(m_flatSettings, m_darkSettings,
                                                             &error);
    }

    if (m_flatField.isValid()) {
        m_flatFieldValid = true;
    } else {
        m_flatFieldError = error;
        if (showWarnings) {
            // 配置缺项是常见状态（还没采平场就勾了 Enable），用状态栏提示即可；
            // 弹模态框会把 OK 之后的流程卡住，自动化测试也难处理。
            showStatusMessage(tr("Flat-field calibration not applied: %1").arg(error), 6000);
        }
    }
}

void MainWindow::applyCalibration(ImageData &frame)
{
    if (!m_darkSettings.enabled && !m_flatSettings.enabled) {
        return;
    }

    const Calibration::FlatField *flat =
        (m_flatSettings.enabled && m_flatFieldValid) ? &m_flatField : nullptr;

    QString reason;
    if (!Calibration::apply(frame, m_darkSettings, flat, &reason)) {
        const quint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - m_lastCalibrationWarnMs >= 1000) {
            m_lastCalibrationWarnMs = nowMs;
            showStatusMessage(tr("Calibration skipped: %1").arg(reason), 2000);
        }
    }
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

void MainWindow::on_despike_toggled(bool checked)
{
    // 去尖峰现在在采集管线里，显示和保存用的是同一份结果；改开关后从
    // 校正后的原图重算一遍当前帧
    rebuildDespikeAndBinning(true);

    if (!checked) {
        showStatusMessage(tr("Despike disabled."), 2000);
        return;
    }

    if (m_lastDespikeResult.sampleCount <= 0) {
        showStatusMessage(tr("Despike on (no rows in the selected row range)."), 3000);
        return;
    }

    showStatusMessage(tr("Despike on: %1 of %2 pixels replaced (shown and saved).")
                          .arg(m_lastDespikeResult.markedCount)
                          .arg(m_lastDespikeResult.sampleCount),
                      4000);
}

void MainWindow::on_despikeSettings_triggered()
{
    if (!m_despikeDialog) {
        m_despikeDialog = new DespikeDialog(this);
        m_despikeDialog->setDespikeParams(m_despikeParams);

        connect(m_despikeDialog, &DespikeDialog::paramsChanged, this, [this]() {
            m_despikeParams = m_despikeDialog->despikeParams();

            // 调参数就是想看效果；去尖峰还没开的话顺手打开，否则改完
            // 画面一点变化都没有，看着像窗口没生效
            if (ui->menuActionDespike->isEnabled() && !ui->menuActionDespike->isChecked()) {
                ui->menuActionDespike->blockSignals(true);
                ui->menuActionDespike->setChecked(true);
                ui->menuActionDespike->blockSignals(false);
            }

            rebuildDespikeAndBinning(true);

            if (m_lastDespikeResult.sampleCount > 0) {
                showStatusMessage(tr("Despike: %1 of %2 pixels replaced.")
                                      .arg(m_lastDespikeResult.markedCount)
                                      .arg(m_lastDespikeResult.sampleCount),
                                  2000);
            }
        });
    } else {
        m_despikeDialog->setDespikeParams(m_despikeParams);
    }

    m_despikeDialog->show();
    m_despikeDialog->raise();
    m_despikeDialog->activateWindow();
}

void MainWindow::updateStateLabel()
{
    QString text = tr("State: %1").arg(m_cameraStateText);
    if (m_captureMode != CaptureMode::None) {
        text += tr(" · Mode: %1").arg(captureModeName(m_captureMode));
    }
    ui->stateLabel->setText(text);
}

QString MainWindow::captureModeName(CaptureMode mode)
{
    switch (mode) {
    case CaptureMode::Live:
        return tr("Live");
    case CaptureMode::Single:
        return tr("Single");
    case CaptureMode::Burst:
        return tr("Burst");
    case CaptureMode::None:
        break;
    }
    return QString();
}

int MainWindow::captureCountForMode(CaptureMode mode) const
{
    // 帧数一律取自 Config 对话框，和「Start」按钮走的是同一份设置。对话框
    // 还没被打开过时没有可读的值，用 CameraTab 的初始值兜底（模式默认
    // Single，帧数默认 5）
    if (m_configDialog) {
        return m_configDialog->getCaptureCount();
    }
    switch (mode) {
    case CaptureMode::Live:
        return 0;
    case CaptureMode::Single:
        return 1;
    case CaptureMode::Burst:
        return 5;
    case CaptureMode::None:
        break;
    }
    return 1;
}

void MainWindow::applyCaptureMode(CaptureMode mode)
{
    m_captureMode = mode;
    updateStateLabel();

    // 先把下拉框改过去再读帧数 —— getCaptureCount() 是按下拉框当前那一项
    // 算出来的，顺序反了读到的就是上一个模式的帧数
    if (m_configDialog) {
        m_configDialog->setCaptureMode(captureModeName(mode));
    }
    requestStartCapture(captureCountForMode(mode));
}

void MainWindow::onLiveModeTriggered()
{
    if (m_appController) {
        applyCaptureMode(CaptureMode::Live);
    }
}

void MainWindow::onSingleModeTriggered()
{
    if (m_appController) {
        applyCaptureMode(CaptureMode::Single);
    }
}

void MainWindow::onBurstModeTriggered()
{
    if (m_appController) {
        applyCaptureMode(CaptureMode::Burst);
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
    // 校准窗口既能采参考帧，也能从文件加载参考帧；没有相机时仍要能打开，
    // 否则离线准备暗帧/平场就没有入口了。Acquire 按钮自己按连接状态禁用。
    ui->menuActionCalibration->setEnabled(true);

    if (m_calibrationDialog) {
        m_calibrationDialog->setAcquireEnabled(connected && !acquiring && !m_acquiringReference);
    }
}



void MainWindow::effectiveDespikeRows(int height, int *startRow, int *endRow) const
{
    if (height <= 0) {
        *startRow = 1;
        *endRow = -1;
        return;
    }

    *startRow = qBound(1, m_vBinStartRow, height);
    *endRow = (m_vBinEndRow < 0) ? height : qBound(1, m_vBinEndRow, height);
}

void MainWindow::rebuildDespikeAndBinning(bool forceDisplay)
{
    if (!m_preDespikeFrame.isValid()) {
        return;
    }

    ImageData frame = m_preDespikeFrame;
    frame.originalImage = QImage();
    m_originalImageIsDespiked = false;
    m_lastDespikeResult = PostProcess::ImageDespikeResult();

    const bool despikeEnabled = ui->menuActionDespike->isChecked()
        && !m_currentFrameIsReference;
    bool roiBinned = false;

    if (despikeEnabled && frame.image.height() > 1) {
        const int height = frame.image.height();
        int startRow = 1;
        int endRow = -1;
        effectiveDespikeRows(height, &startRow, &endRow);

        const bool fullRange = (startRow <= 1 && endRow >= height);
        const bool partialRange = (startRow <= endRow) && !fullRange;

        if (m_vBinEnabled && partialRange) {
            // vbin 只看谱带那几行：只把 ROI（前后各多带一行做邻域）复制出来
            // 去尖峰，再对 ROI 做合并。这样每帧不用为了显示复制整幅 48 MB，
            // 完整二维图留给保存时再补
            const int roiFirst = startRow - 1;
            const int roiLast = endRow - 1;
            const int haloFirst = std::max(0, roiFirst - 1);
            const int haloLast = std::min(height - 1, roiLast + 1);

            const QImage roi = frame.image.copy(0, haloFirst, frame.image.width(),
                                                haloLast - haloFirst + 1);
            if (!roi.isNull()) {
                const int roiFirstInRoi = roiFirst - haloFirst;
                const int roiLastInRoi = roiLast - haloFirst;

                const QImage processed = PostProcess::despikeImage(roi, m_despikeParams,
                                                                   &m_lastDespikeResult,
                                                                   roiFirstInRoi, roiLastInRoi);

                ImageData binned = frame;
                binned.image = processed;
                binned.originalImage = QImage();
                PostProcess::verticalBinning(binned, roiFirstInRoi + 1, roiLastInRoi + 1);

                frame.image = binned.image;
                frame.spectrum = binned.spectrum;
                // originalImage 留原始二维图；saveFrameToFile() 会在保存时按同一
                // 行范围补做去尖峰，范围内的数据不会漏掉
                frame.originalImage = m_preDespikeFrame.image;
                roiBinned = true;
            }
        }

        if (!roiBinned) {
            frame.image = PostProcess::despikeImage(frame.image, m_despikeParams,
                                                    &m_lastDespikeResult,
                                                    startRow - 1,
                                                    endRow < 0 ? -1 : endRow - 1);
        }
    }

    if (m_vBinEnabled && !roiBinned) {
        // 保存/加载都以 originalImage 为主图，这里必须是去尖峰之后的 2D 图
        m_originalImageIsDespiked = despikeEnabled && frame.image.height() > 1;
        frame.originalImage = frame.image;
        PostProcess::verticalBinning(frame, m_vBinStartRow, m_vBinEndRow);
    }

    m_currentFrame = frame;
    updateDisplay(m_currentFrame, forceDisplay);
}

void MainWindow::updateDisplay(const ImageData &frame, bool force)
{
    if (!frame.isValid()) {
        return;
    }

    if (!force && !m_frameTimer.hasExpired(MIN_FRAME_INTERVAL_MS)) {
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
            // 和图像视图保持同一份数据：去尖峰开着时统计的也是修补后的图，
            // 否则右侧看到的和直方图对不上
            m_histogramWindow->setImage(m_imageViewWidget->image());
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

    // 直方图只统计图像，去尖峰两种视图都有：直方图菜单项跟着视图走，
    // 去尖峰的两个菜单项在第一帧确定视图后就可以一直放开。只在模式真的变了
    // 时才动，switchView 是每帧都调的，没必要每帧都去碰一遍 QAction。
    // 首帧必须走一次，菜单项的初始状态都是禁用的（见 MainWindowUi）
    const bool imageViewActive = (height != 1);
    if (!m_viewModeKnown) {
        m_viewModeKnown = true;
        m_imageViewActive = imageViewActive;
        ui->menuActionDespike->setEnabled(true);
        ui->menuActionDespikeSettings->setEnabled(true);
        ui->menuActionHistogram->setEnabled(imageViewActive);
        return;
    }
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