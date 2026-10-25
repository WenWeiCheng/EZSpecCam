#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
#include <QShortcut>
#include <QSettings>
#include <QThread>

#include "../AppController.h"
#include "../Theme.h"
#include "../workers/FileLoaderWorker.h"
#include "Calibration.h"
#include "PostProcess.h"

class MainWindowUi;
class CameraTab;
class CameraConfigDialog;
class ImageViewWidget;
class LineViewWidget;
class ProfileWindow;
class HistogramWindow;
class ScaleControlDialog;
class DisplayStyleDialog;
class CalibrationDialog;
class DespikeDialog;
class FileSaverWorker;
struct ImageData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    MainWindowUi *getUi() const { return ui; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_actionSaveFrameAs_triggered();
    void on_actionSaveFrame_triggered();
    void on_actionAutoSaveToggle_triggered(bool checked);
    void on_actionChangeAutoSaveDir_triggered();
    void on_actionConfig_triggered();
    void on_actionAbout_triggered();
    void on_actionStart_triggered();
    void on_actionStop_triggered();
    void on_actionOpenFrame_triggered();
    void onFrameLoaded(const LoadResult &result, const QString &filePath);
    void onFrameLoadFailed(const QString &error, const QString &filePath);

    void on_scale_triggered();

    void on_showAxes_triggered(bool checked);
    void on_display_style_triggered();
    void on_fillWindow_triggered(bool checked);
    void on_statistics_triggered();
    void on_verticalBinning_triggered();
    void on_rowRange_triggered();
    void on_profile_triggered();
    void on_histogram_triggered();
    void on_despike_toggled(bool checked);
    void on_despikeSettings_triggered();

    void onCameraStateChanged(CameraState newState);
    void onCameraFrameReady(const ImageData &frame);
    void onConnectionChanged(bool connected);
    void onErrorOccurred(const CameraError &error);
    void onCaptureStarted();
    void onCaptureStopped();

    void onFileSaveCompleted(const QString &);
    void onFileSaveFailed(const QString &error, const QString &details);

    void onCrosshairCleared();
    void onCrosshairMoved(const QPointF &position, int value);

    void onLiveModeTriggered();
    void onSingleModeTriggered();
    void onBurstModeTriggered();

    void on_actionCalibration_triggered();
    void onCalibrationApplied(bool enabled, const QString &path, int bias);
    void onAcquireDarkFrameStartRequested(int frameCount);
    void onReferenceFileSelected(int kind, const QString &path);
    void onSaveReferenceRequested(int kind);
    void onSaveVffEntryRequested(int index);
    void onVffRemoveRequested(int index);
    void onVffClearRequested();

    //! 组装 SaveRequest 并交给 FileSaverWorker。做成槽是为了让集成测试能
    //! 直接保存到临时路径，验证去尖峰结果确实进了文件
    void saveFrameToFile(const QString &filePath);
    //! 把内存里（或从文件加载的）校准参考帧另存到 @p filePath。同样做成槽，
    //! 方便测试绕过文件对话框直接验证写盘内容
    void saveReferenceFrameToFile(int kind, const QString &filePath);
    void saveVffEntryToFile(int index, const QString &filePath);

private:
    // L / S / B 选中的采集模式。这是常驻状态，挂在状态栏那条常驻带子上；
    // 早先它走 QStatusBar::showMessage() 当 3 秒提示，而那条提示会把常驻
    // widget 全藏起来，随后 setCrosshairInfo() 又无条件 setVisible(true)
    // 把十字线读数拉回同一块矩形，两段文字就叠在了一起。
    //
    // 出处仍然是 Config 对话框里的「Capture Mode」下拉框：快捷键改的就是
    // 那个下拉框，这里只是把它读回来显示。默认 Single，和下拉框的初始值一致，
    // 所以对话框还没打开过时状态栏上也是有内容的。
    enum class CaptureMode { None, Live, Single, Burst };

    void restoreTheme();
    void setThemeMode(Theme::Mode mode);
    void syncThemeMenu();
    void updateToolbarState();
    void updateDisplay(const ImageData &frame, bool force = false);
    //! 从去尖峰之前的帧重建当前帧：去尖峰 → 软件垂直合并 → 更新显示
    void rebuildDespikeAndBinning(bool forceDisplay = false);
    //! 去尖峰真正生效的 1-based 闭区间；endRow 为 -1 表示到最后一行
    void effectiveDespikeRows(int height, int *startRow, int *endRow) const;
    void switchView(int height);
    void updateFpsDisplay();
    void onFpsTimerTimeout();
    void showStatusMessage(const QString &message, int timeoutMs = 3000);
    void startReferenceAcquisition(Calibration::ReferenceKind kind, int frameCount,
                                   double exposure);
    void cancelReferenceAcquisition(const QString &reason);
    void storeReferenceFrame(Calibration::ReferenceKind kind,
                             const Calibration::ReferenceFrame &reference);
    void rebuildFlatField(bool showWarnings);
    void applyCalibration(ImageData &frame);
    QString referenceName(Calibration::ReferenceKind kind) const;
    //! Reference card held in the settings, or nullptr for VffEntry/unknown.
    const Calibration::ReferenceFrame *referenceForKind(Calibration::ReferenceKind kind) const;
    //! Common Save As path picker: remembers the last directory in QSettings.
    QString askReferenceSavePath(Calibration::ReferenceKind kind,
                                 const Calibration::ReferenceFrame &reference,
                                 const QString &stem);
    void saveReferencePlane(Calibration::ReferenceKind kind,
                            const Calibration::ReferenceFrame &reference,
                            const QString &filePath);
    void requestStartCapture(int captureCount);
    void refreshCalibrationDialog(bool reloadConfig = false);
    void updateStateLabel();
    void applyCaptureMode(CaptureMode mode);
    int captureCountForMode(CaptureMode mode) const;
    static QString captureModeName(CaptureMode mode);

    QElapsedTimer m_frameTimer;
    static constexpr int MIN_FRAME_INTERVAL_MS = 33;

    QTimer *m_fpsTimer = nullptr;
    int m_fpsFrameCount = 0;
    int m_fpsValue = 0;

    ImageData m_currentFrame;
    //! 校正之后、去尖峰和垂直合并之前的帧。切换去尖峰、参数或行范围时
    //! 都从这份数据重新算，避免在已经处理过的结果上叠加
    ImageData m_preDespikeFrame;
    //! 当前 m_currentFrame 是否是校准参考帧（参考帧不参与去尖峰）
    bool m_currentFrameIsReference = false;
    //! m_currentFrame.originalImage 是否已经去过尖峰。部分行范围 + vbin 时
    //! 管线只处理 ROI，originalImage 保持原样，保存时再补一次
    bool m_originalImageIsDespiked = false;
    PostProcess::DespikeParams m_despikeParams;
    PostProcess::ImageDespikeResult m_lastDespikeResult;

    MainWindowUi *ui;
    AppController *m_appController;
    QThread *m_controllerThread = nullptr;
    QThread *m_fileSaverThread = nullptr;
    QThread *m_fileLoaderThread = nullptr;
    FileSaverWorker *m_fileSaverWorker = nullptr;
    FileLoaderWorker *m_fileLoaderWorker = nullptr;
    CameraConfigDialog *m_configDialog = nullptr;
    ProfileWindow *m_profileWindow = nullptr;
    HistogramWindow *m_histogramWindow = nullptr;
    DespikeDialog *m_despikeDialog = nullptr;
    ScaleControlDialog *m_scaleDialog = nullptr;
    DisplayStyleDialog *m_displayStyleDialog = nullptr;
    CalibrationDialog *m_calibrationDialog = nullptr;

    int m_frameCount = 0;
    int m_autoSaveFrameCounter = 0;
    //! 当前是否停在 imageView。直方图入口只在这个模式下可点
    bool m_imageViewActive = false;
    //! switchView 是否已经跑过至少一次。首帧之前两种视图都还没有数据，光靠
    //! m_imageViewActive 的初值分不清「还没定过」和「确定停在 spectrumView」，
    //! 首帧就是光谱时菜单项会永远等不到启用它的那一次
    bool m_viewModeKnown = false;

    // 状态栏那一格显示的东西：相机状态 + 当前采集模式
    QString m_cameraStateText;
    CaptureMode m_captureMode = CaptureMode::Single;

    QDateTime m_launchTimestamp;
    QString m_autoSaveDir;

    bool m_vBinEnabled = false;
    //! 行范围，1-based 闭区间（与 metadata 的 vBinStartRow/vBinEndRow 一致）。
    //! PostProcess::verticalBinning 也按 1-based 解释，调用处不要做转换；
    //! 只有 RowRangeDialog 是 0-based（和 Profile 的行号一致），边界在
    //! on_rowRange_triggered 里转换
    int m_vBinStartRow = 1;
    //! -1 表示「到最后一行」
    int m_vBinEndRow = -1;

    // ----- Calibration state -------------------------------------------
    // The applied configuration is authoritative here; the dialog is only an
    // editor. Reference planes stay in memory until the application exits.
    Calibration::DarkSettings m_darkSettings;
    Calibration::FlatSettings m_flatSettings;
    Calibration::FlatField m_flatField;
    bool m_flatFieldValid = false;
    QString m_flatFieldError;
    quint64 m_lastCalibrationWarnMs = 0;

    // ----- Reference acquisition state machine -------------------------
    bool m_acquiringReference = false;
    Calibration::ReferenceKind m_acquiringKind = Calibration::ReferenceKind::MatchedDark;
    double m_acquiringExposure = 0.0;
    int m_referenceBurstTotal = 10;
    int m_referenceBurstRemaining = 0;
    bool m_referenceAccumInit = false;
    quint64 m_referenceAccumFrames = 0;
    int m_referenceAccumWidth = 0;
    int m_referenceAccumHeight = 0;
    QImage::Format m_referenceAccumFormat = QImage::Format_Invalid;
    // Per-pixel (per-channel for RGB) running sum. quint64 keeps the accumulator
    // safe from wrap-around across the full range of supported frame counts
    // (CalibrationDialog::kMaxFrameCount) and pixel depths (8/16-bit).
    QVector<quint64> m_referenceAccumSum;

    // When a reference image is being loaded from disk, onFrameLoaded stores it
    // into this slot instead of displaying it as the current frame.
    bool m_loadingReference = false;
    Calibration::ReferenceKind m_loadingReferenceKind = Calibration::ReferenceKind::MatchedDark;
    double m_loadingReferenceExposure = 0.0;
    QString m_loadingReferencePath;

    QShortcut *shortcutLive = nullptr;
    QShortcut *shortcutSingle = nullptr;
    QShortcut *shortcutBurst = nullptr;
    CameraTab *m_cameraTab;
    ImageViewWidget *m_imageViewWidget;
    LineViewWidget *m_spectrumViewWidget;
};


#endif // MAINWINDOW_H