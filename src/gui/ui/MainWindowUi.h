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

    QAction *actionConfig;
    QAction *actionAbout;
    QAction *actionStart;
    QAction *actionStop;

    QAction *menuActionSaveFrameAs;
    QAction *menuActionSaveFrame;
    QAction *menuActionAutoSaveToggle;
    QAction *menuActionChangeAutoSaveDir;

    QAction *menuActionConfig;
    QAction *menuActionAbout;
    QAction *toolbarActionConfig;

    QAction *menuActionScale;

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