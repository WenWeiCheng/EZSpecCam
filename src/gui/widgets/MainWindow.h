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

class MainWindowUi;
class CameraTab;
class CameraConfigDialog;
class ImageViewWidget;
class SpectrumViewWidget;
class ProfileWindow;
class HistogramWindow;
class ScaleControlDialog;
class DisplayStyleDialog;
class CalibrationDialog;
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
    void updateDisplay(const ImageData &frame);
    void switchView(int height);
    void updateFpsDisplay();
    void onFpsTimerTimeout();
    void showStatusMessage(const QString &message, int timeoutMs = 3000);
    void saveFrameToFile(const QString &filePath);
    void cancelDarkAcquisition(const QString &reason);
    void requestStartCapture(int captureCount);
    void refreshCalibrationDialog();
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
    ScaleControlDialog *m_scaleDialog = nullptr;
    DisplayStyleDialog *m_displayStyleDialog = nullptr;
    CalibrationDialog *m_calibrationDialog = nullptr;

    int m_frameCount = 0;
    int m_autoSaveFrameCounter = 0;
    //! 当前是否停在 imageView。直方图入口只在这个模式下可点
    bool m_imageViewActive = false;

    // 状态栏那一格显示的东西：相机状态 + 当前采集模式
    QString m_cameraStateText;
    CaptureMode m_captureMode = CaptureMode::Single;

    QDateTime m_launchTimestamp;
    QString m_autoSaveDir;

    bool m_vBinEnabled = false;
    int m_vBinStartRow = 0;
    int m_vBinEndRow = -1;

    // Dark-frame calibration state
    bool    m_darkEnabled  = false;
    QString m_darkPath;
    int     m_darkBias     = 1000;
    QImage  m_darkFrame;
    bool    m_darkFrameValid = false;

    // Dark-frame acquisition state machine
    bool    m_acquiringDark  = false;
    int     m_darkBurstTotal     = 10;
    int     m_darkBurstRemaining = 0;
    bool    m_darkAccumInit     = false;
    quint64 m_darkAccumFrames   = 0;
    quint64 m_lastDarkSizeWarnMs = 0;
    QImage::Format m_darkAccumFormat = QImage::Format_Invalid;
    // Per-pixel (per-channel for RGB) running sum. quint64 keeps the accumulator
    // safe from wrap-around across the full range of supported frame counts
    // (CalibrationDialog::kMaxFrameCount) and pixel depths (8/16-bit).
    QVector<quint64> m_darkAccumSum;

    // Discriminator: when true, onFrameLoaded stores result as m_darkFrame
    bool    m_loadingFrameIsDark = false;

    QShortcut *shortcutLive = nullptr;
    QShortcut *shortcutSingle = nullptr;
    QShortcut *shortcutBurst = nullptr;
    CameraTab *m_cameraTab;
    ImageViewWidget *m_imageViewWidget;
    SpectrumViewWidget *m_spectrumViewWidget;
};


#endif // MAINWINDOW_H