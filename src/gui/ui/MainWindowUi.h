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

    QAction *actionSaveFrame;
    QAction *actionConfig;
    QAction *actionAbout;
    QAction *actionStart;
    QAction *actionStop;

    QAction *menuActionSaveFrameAs;
    QAction *menuActionSaveFrameAutoNumber;
    QAction *menuActionAutoSaveToggle;
    QAction *menuActionChangeAutoSaveDir;

    QAction *menuActionSaveFrame;
    QAction *menuActionConfig;
    QAction *menuActionAbout;
    QAction *toolbarActionConfig;

    QAction *menuActionColorScale;
    QAction *menuActionSpectrumRange;

    QAction *menuActionColorScaleAuto;
    QAction *menuActionColorScale8Bit;
    QAction *menuActionColorScale16Bit;

    QAction *menuActionSpectrumRangeAuto;
    QAction *menuActionSpectrumRangeFull;
    QAction *menuActionSpectrumRangeZoomLeft;
    QAction *menuActionSpectrumRangeZoomRight;
    QAction *menuActionSpectrumRangeZoomCenter;
    QAction *menuActionSpectrumRangeCustom;

    QAction *menuActionShowAxes;
    QAction *menuActionProfile;

    QAction *menuActionStatistics;
    QAction *menuActionPostProcess;
    QAction *menuActionVerticalBinning;
    QAction *menuActionRowRange;

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