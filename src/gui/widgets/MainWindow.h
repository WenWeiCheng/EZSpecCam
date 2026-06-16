#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>
#include <QTimer>
#include <QShortcut>
#include <QSettings>
#include <QThread>

#include "../AppController.h"

class MainWindowUi;
class CameraTab;
class CameraConfigDialog;
class ImageViewWidget;
class SpectrumViewWidget;
class ProfileWindow;
class ScaleControlDialog;
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

    void on_scale_triggered();

    void on_showAxes_triggered(bool checked);
    void on_fillWindow_triggered(bool checked);
    void on_statistics_triggered();
    void on_verticalBinning_triggered();
    void on_rowRange_triggered();
    void on_profile_triggered();

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
    void onOverexposureDetected(QPoint position);

    void onLiveModeTriggered();
    void onSingleModeTriggered();
    void onBurstModeTriggered();

private:
    void updateToolbarState();
    void updateDisplay(const ImageData &frame);
    void switchView(int height);
    void updateFpsDisplay();
    void onFpsTimerTimeout();
    void showStatusMessage(const QString &message, int timeoutMs = 3000);
    void saveFrameToFile(const QString &filePath);

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
    FileSaverWorker *m_fileSaverWorker = nullptr;
    CameraTab *m_cameraTab;
    ImageViewWidget *m_imageViewWidget;
    SpectrumViewWidget *m_spectrumViewWidget;
    CameraConfigDialog *m_configDialog = nullptr;
    ProfileWindow *m_profileWindow = nullptr;
    ScaleControlDialog *m_scaleDialog = nullptr;

    int m_frameCount = 0;
    int m_autoSaveFrameCounter = 0;

    QDateTime m_launchTimestamp;
    QString m_autoSaveDir;

    bool m_vBinEnabled = false;
    int m_vBinStartRow = 0;
    int m_vBinEndRow = -1;
};

#endif // MAINWINDOW_H