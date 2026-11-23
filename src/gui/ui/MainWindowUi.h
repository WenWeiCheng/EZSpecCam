#ifndef MAINWINDOWUI_H
#define MAINWINDOWUI_H

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QObject>
#include <QStackedWidget>
#include <QString>
#include <QToolBar>

//! Layout-only helper for MainWindow: menus, toolbar, status bar and the
//! central stack. The image/spectrum views are created by MainWindow and passed
//! in, so this class does not depend on widgets/display.
class MainWindowUi : public QObject
{
    Q_OBJECT
public:
    explicit MainWindowUi(QObject *parent = nullptr);
    ~MainWindowUi();

    void setupUi(QMainWindow *mainWindow, QWidget *imageViewWidget,
                 QWidget *spectrumViewWidget);

    // 十字线读数。没有十字线时传空串：标签整个隐藏，而不是留一条
    // "Crosshair: --" 占着状态栏的位置
    void setCrosshairInfo(const QString &text);

    QAction *actionStart = nullptr;
    QAction *actionStop = nullptr;

    QAction *menuActionSaveFrameAs = nullptr;
    QAction *menuActionSaveFrame = nullptr;
    QAction *menuActionAutoSaveToggle = nullptr;
    QAction *menuActionChangeAutoSaveDir = nullptr;
    QAction *menuActionOpenFrame = nullptr;

    QAction *menuActionConfig = nullptr;
    QAction *menuActionAbout = nullptr;
    QAction *toolbarActionConfig = nullptr;

    QAction *menuActionScale = nullptr;

    QAction *menuActionShowAxes = nullptr;
    QAction *menuActionFillWindow = nullptr;
    QAction *menuActionDisplayStyle = nullptr;
    QAction *menuActionProfile = nullptr;
    QAction *menuActionHistogram = nullptr;

    QAction *menuActionThemeSystem = nullptr;
    QAction *menuActionThemeLight = nullptr;
    QAction *menuActionThemeDark = nullptr;

    QAction *menuActionStatistics = nullptr;
    QAction *menuActionPostProcess = nullptr;
    QAction *menuActionVerticalBinning = nullptr;
    QAction *menuActionRowRange = nullptr;
    QAction *menuActionDespike = nullptr;
    QAction *menuActionDespikeSettings = nullptr;
    QAction *menuActionCalibration = nullptr;

    QLabel *stateLabel = nullptr;
    QLabel *frameCountLabel = nullptr;
    QLabel *fpsLabel = nullptr;
    QLabel *coordLabel = nullptr;
    QToolBar *toolBar = nullptr;
    QStackedWidget *centralStackedWidget = nullptr;

    void setupCentralWidget(QMainWindow *mainWindow, QWidget *imageViewWidget,
                            QWidget *spectrumViewWidget);

private:
    void createMenuBar(QMainWindow *mainWindow);
    void createToolBar(QMainWindow *mainWindow);
    void createStatusBar(QMainWindow *mainWindow);
};

#endif // MAINWINDOWUI_H
