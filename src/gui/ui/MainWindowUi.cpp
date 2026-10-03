#include "MainWindowUi.h"
#include "../widgets/display/ImageViewWidget.h"
#include "../widgets/display/SpectrumViewWidget.h"

#include <QMenuBar>
#include <QMenu>
#include <QMainWindow>
#include <QIcon>
#include <QActionGroup>
#include <QApplication>

MainWindowUi::MainWindowUi(QObject *parent)
    : m_parent(parent)
    , actionConfig(nullptr)
    , actionAbout(nullptr)
    , actionStart(nullptr)
    , actionStop(nullptr)
    , stateLabel(nullptr)
    , frameCountLabel(nullptr)
    , fpsLabel(nullptr)
    , coordLabel(nullptr)
    , toolBar(nullptr)
    , menuActionSaveFrame(nullptr)
    , menuActionConfig(nullptr)
    , menuActionAbout(nullptr)
    , toolbarActionConfig(nullptr)
    , centralStackedWidget(nullptr)
    , imageViewWidget(nullptr)
    , spectrumViewWidget(nullptr)
    , menuActionShowAxes(nullptr)
    , menuActionFillWindow(nullptr)
    , menuActionDisplayStyle(nullptr)
    , menuActionProfile(nullptr)
    , menuActionHistogram(nullptr)
    , menuActionStatistics(nullptr)
    , menuActionPostProcess(nullptr)
    , menuActionVerticalBinning(nullptr)
    , menuActionRowRange(nullptr)
    , menuActionAcquireDarkFrame(nullptr)
    , menuActionCalibration(nullptr)
    , menuActionSaveFrameAs(nullptr)
    , menuActionAutoSaveToggle(nullptr)
    , menuActionChangeAutoSaveDir(nullptr)
    , menuActionOpenFrame(nullptr)
    , menuActionScale(nullptr)
{
}

MainWindowUi::~MainWindowUi()
{
}

void MainWindowUi::setupUi(QMainWindow *mainWindow)
{
    if (!mainWindow) {
        return;
    }

    mainWindow->setWindowTitle("EZSpecCam");
    mainWindow->setMinimumSize(800, 600);

    createMenuBar(mainWindow);
    createToolBar(mainWindow);
    createStatusBar(mainWindow);
    setupCentralWidget(mainWindow);
}

void MainWindowUi::createMenuBar(QMainWindow *mainWindow)
{
    QMenuBar *menuBar = mainWindow->menuBar();

    QMenu *menuFile = menuBar->addMenu("&File");

    menuActionOpenFrame = new QAction("&Open Frame...", mainWindow);
    menuActionOpenFrame->setShortcut(QKeySequence::Open);
    menuFile->addAction(menuActionOpenFrame);

    menuFile->addSeparator();

    menuActionSaveFrameAs = new QAction("Save Frame As...", mainWindow);
    menuActionSaveFrameAs->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    menuFile->addAction(menuActionSaveFrameAs);

    menuActionSaveFrame = new QAction("Save Frame", mainWindow);
    menuActionSaveFrame->setShortcut(QKeySequence(Qt::ALT | Qt::Key_S));
    menuFile->addAction(menuActionSaveFrame);

    menuFile->addSeparator();

    menuActionAutoSaveToggle = new QAction("Auto Save", mainWindow);
    menuActionAutoSaveToggle->setCheckable(true);
    menuFile->addAction(menuActionAutoSaveToggle);

    menuActionChangeAutoSaveDir = new QAction("Change Auto-Save Directory...", mainWindow);
    menuFile->addAction(menuActionChangeAutoSaveDir);

    menuFile->addSeparator();

    menuActionSaveFrame = menuActionSaveFrame;

    QMenu *menuCamera = menuBar->addMenu("&Camera");
    menuActionConfig = new QAction("&Config", mainWindow);
    menuActionConfig->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
    menuCamera->addAction(menuActionConfig);
    actionConfig = menuActionConfig;

    menuCamera->addSeparator();
    menuActionAcquireDarkFrame = new QAction("Acquire Dark Frame", mainWindow);
    menuCamera->addAction(menuActionAcquireDarkFrame);

    QMenu *menuAnalyse = menuBar->addMenu("&Analyse");
    menuActionStatistics = new QAction("&Statistics", mainWindow);
    menuActionStatistics->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    menuAnalyse->addAction(menuActionStatistics);

    QMenu *menuView = menuBar->addMenu("&View");
    menuActionShowAxes = new QAction("Show Image Axes", mainWindow);
    menuActionShowAxes->setCheckable(true);
    menuActionShowAxes->setChecked(false);
    menuView->addAction(menuActionShowAxes);

    menuActionFillWindow = new QAction("Fill Window", mainWindow);
    menuActionFillWindow->setCheckable(true);
    menuActionFillWindow->setChecked(false);
    menuView->addAction(menuActionFillWindow);

    menuActionScale = new QAction("&Scale...", mainWindow);
    menuView->addAction(menuActionScale);

    menuActionDisplayStyle = new QAction("Display Style...", mainWindow);
    menuView->addAction(menuActionDisplayStyle);

    menuActionProfile = new QAction("&Profile", mainWindow);
    menuActionProfile->setShortcut(QKeySequence(Qt::Key_P));
    menuActionProfile->setShortcutContext(Qt::ApplicationShortcut);
    menuView->addAction(menuActionProfile);

    menuActionHistogram = new QAction("&Histogram", mainWindow);
    menuActionHistogram->setShortcut(QKeySequence(Qt::Key_H));
    menuActionHistogram->setShortcutContext(Qt::ApplicationShortcut);
    // 直方图统计的是图像，spectrumView 模式下没有可统计的对象，
    // 等切到 imageView 再由 MainWindow::switchView 放开
    menuActionHistogram->setEnabled(false);
    menuView->addAction(menuActionHistogram);

    menuView->addSeparator();
    QMenu *menuTheme = menuView->addMenu("&Theme");
    // 用 QActionGroup 做互斥，选中哪一项就是当前的配色来源
    QActionGroup *themeGroup = new QActionGroup(mainWindow);
    menuActionThemeSystem = new QAction("&Follow System", themeGroup);
    menuActionThemeSystem->setCheckable(true);
    menuActionThemeLight = new QAction("&Light", themeGroup);
    menuActionThemeLight->setCheckable(true);
    menuActionThemeDark = new QAction("&Dark", themeGroup);
    menuActionThemeDark->setCheckable(true);
    themeGroup->addAction(menuActionThemeSystem);
    themeGroup->addAction(menuActionThemeLight);
    themeGroup->addAction(menuActionThemeDark);
    menuTheme->addActions(themeGroup->actions());
    // 默认跟着系统，设置在 MainWindow 里同步过来
    menuActionThemeSystem->setChecked(true);

    QMenu *menuPostProcess = menuBar->addMenu("&Post-Process");
    menuActionVerticalBinning = new QAction("Software Vertical Binning", mainWindow);
    menuActionVerticalBinning->setShortcut(QKeySequence(Qt::Key_V));
    menuActionVerticalBinning->setShortcutContext(Qt::ApplicationShortcut);
    menuActionVerticalBinning->setCheckable(true);
    menuPostProcess->addAction(menuActionVerticalBinning);

    menuActionRowRange = new QAction("Row Range...", mainWindow);
    menuPostProcess->addAction(menuActionRowRange);

    menuPostProcess->addSeparator();
    menuActionCalibration = new QAction("Calibration...", mainWindow);
    menuPostProcess->addAction(menuActionCalibration);
    QMenu *menuHelp = menuBar->addMenu("&Help");
    menuActionAbout = new QAction("&About", mainWindow);
    menuHelp->addAction(menuActionAbout);
    actionAbout = menuActionAbout;
}

void MainWindowUi::createToolBar(QMainWindow *mainWindow)
{
    toolBar = new QToolBar("Main Toolbar", mainWindow);
    toolBar->setMovable(false);
    mainWindow->addToolBar(toolBar);

    toolbarActionConfig = new QAction(QIcon(), "Config", mainWindow);
    toolbarActionConfig->setToolTip("Configure camera settings");
    toolBar->addAction(toolbarActionConfig);

    toolBar->addSeparator();

    actionStart = new QAction(QIcon(), "Start", mainWindow);
    actionStart->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    actionStart->setToolTip("Start capture (Ctrl+R)");
    toolBar->addAction(actionStart);

    actionStop = new QAction(QIcon(), "Stop", mainWindow);
    actionStop->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    actionStop->setToolTip("Stop capture (Ctrl+T)");
    toolBar->addAction(actionStop);
}

void MainWindowUi::createStatusBar(QMainWindow *mainWindow)
{
    QStatusBar *statusBar = mainWindow->statusBar();

    stateLabel = new QLabel("State: Disconnected", statusBar);
    stateLabel->setMinimumWidth(150);
    statusBar->addWidget(stateLabel);

    coordLabel = new QLabel("Crosshair: --", statusBar);
    coordLabel->setMinimumWidth(150);
    statusBar->addWidget(coordLabel);

    fpsLabel = new QLabel("FPS: 0", statusBar);
    fpsLabel->setMinimumWidth(80);
    statusBar->addPermanentWidget(fpsLabel);

    frameCountLabel = new QLabel("Frames: 0", statusBar);
    frameCountLabel->setAlignment(Qt::AlignRight);
    statusBar->addPermanentWidget(frameCountLabel);
}

void MainWindowUi::initializeLabels()
{
    if (stateLabel) {
        stateLabel->setText("State: Disconnected");
    }
    if (frameCountLabel) {
        frameCountLabel->setText("Frames: 0");
    }
    if (fpsLabel) {
        fpsLabel->setText("FPS: 0");
    }
    if (coordLabel) {
        coordLabel->setText("Crosshair: --");
    }
}

void MainWindowUi::setupCentralWidget(QMainWindow *mainWindow)
{
    centralStackedWidget = new QStackedWidget(mainWindow);

    imageViewWidget = new ImageViewWidget(centralStackedWidget);
    spectrumViewWidget = new SpectrumViewWidget(centralStackedWidget);

    imageViewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    spectrumViewWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    centralStackedWidget->addWidget(imageViewWidget);
    centralStackedWidget->addWidget(spectrumViewWidget);

    mainWindow->setCentralWidget(centralStackedWidget);
}