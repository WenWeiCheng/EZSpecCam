#ifndef MAINWINDOWUI_H
#define MAINWINDOWUI_H

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QAction>
#include <QIcon>
#include <QObject>
#include <QMainWindow>
#include <QStackedWidget>

class ImageViewWidget;
class SpectrumViewWidget;

class MainWindowUi : public QObject
{
    Q_OBJECT
public:
    explicit MainWindowUi(QObject *parent = nullptr);
    ~MainWindowUi();

    void setupUi(QMainWindow *mainWindow);

    // 十字线读数。没有十字线时传空串：标签整个隐藏，而不是留一条
    // "Crosshair: --" 占着状态栏的位置
    void setCrosshairInfo(const QString &text);

    QAction *actionConfig;
    QAction *actionAbout;
    QAction *actionStart;
    QAction *actionStop;

    QAction *menuActionSaveFrameAs;
    QAction *menuActionSaveFrame;
    QAction *menuActionAutoSaveToggle;
    QAction *menuActionChangeAutoSaveDir;
    QAction *menuActionOpenFrame;

    QAction *menuActionConfig;
    QAction *menuActionAbout;
    QAction *toolbarActionConfig;

    QAction *menuActionScale;

    QAction *menuActionShowAxes;
    QAction *menuActionFillWindow;
    QAction *menuActionDisplayStyle;
    QAction *menuActionProfile;
    QAction *menuActionHistogram;

    QAction *menuActionThemeSystem;
    QAction *menuActionThemeLight;
    QAction *menuActionThemeDark;

    QAction *menuActionStatistics;
    QAction *menuActionPostProcess;
    QAction *menuActionVerticalBinning;
    QAction *menuActionRowRange;
    QAction *menuActionCalibration;

    QLabel *stateLabel;
    QLabel *frameCountLabel;
    QLabel *fpsLabel;
    QLabel *coordLabel;
    QToolBar *toolBar;
    QStackedWidget *centralStackedWidget;
    ImageViewWidget *imageViewWidget;
    SpectrumViewWidget *spectrumViewWidget;

    void setupCentralWidget(QMainWindow *mainWindow);

private:
    void createMenuBar(QMainWindow *mainWindow);
    void createToolBar(QMainWindow *mainWindow);
    void createStatusBar(QMainWindow *mainWindow);
    void initializeLabels();

    QObject *m_parent;
};

#endif // MAINWINDOWUI_H