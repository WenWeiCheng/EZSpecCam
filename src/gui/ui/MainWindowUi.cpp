#include "MainWindowUi.h"
#include "../widgets/ImageViewWidget.h"
#include "../widgets/SpectrumViewWidget.h"

#include <QMenuBar>
#include <QMenu>
#include <QMainWindow>
#include <QIcon>
#include <QApplication>

MainWindowUi::MainWindowUi(QObject *parent)
    : m_parent(parent)
    , actionSaveFrame(nullptr)
    , actionConfig(nullptr)
    , actionAbout(nullptr)
    , actionStart(nullptr)
    , actionStop(nullptr)
    , stateLabel(nullptr)
    , frameCountLabel(nullptr)
    , fpsLabel(nullptr)
    , toolBar(nullptr)
    , menuActionSaveFrame(nullptr)
    , menuActionConfig(nullptr)
    , menuActionAbout(nullptr)
    , toolbarActionConfig(nullptr)
    , centralStackedWidget(nullptr)
    , imageViewWidget(nullptr)
    , spectrumViewWidget(nullptr)
    , menuActionColorScale(nullptr)
    , menuActionSpectrumRange(nullptr)
    , menuActionColorScaleAuto(nullptr)
    , menuActionColorScale8Bit(nullptr)
    , menuActionColorScale16Bit(nullptr)
    , menuActionSpectrumRangeAuto(nullptr)
    , menuActionSpectrumRangeFull(nullptr)
    , menuActionSpectrumRangeZoomLeft(nullptr)
    , menuActionSpectrumRangeZoomRight(nullptr)
    , menuActionSpectrumRangeZoomCenter(nullptr)
    , menuActionSpectrumRangeCustom(nullptr)
    , menuActionShowAxes(nullptr)
    , menuActionStatistics(nullptr)
    , menuActionPostProcess(nullptr)
    , menuActionVerticalBinning(nullptr)
    , menuActionRowRange(nullptr)
    , menuActionSaveFrameAs(nullptr)
    , menuActionSaveFrameAutoNumber(nullptr)
    , menuActionAutoSaveToggle(nullptr)
    , menuActionChangeAutoSaveDir(nullptr)
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

    menuActionSaveFrameAs = new QAction("Save Frame As...", mainWindow);
    menuActionSaveFrameAs->setShortcut(QKeySequence(Qt::ALT | Qt::Key_S));
    menuFile->addAction(menuActionSaveFrameAs);

    menuActionSaveFrameAutoNumber = new QAction("Save Frame (Auto Number)", mainWindow);
    menuActionSaveFrameAutoNumber->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    menuFile->addAction(menuActionSaveFrameAutoNumber);

    menuFile->addSeparator();

    menuActionAutoSaveToggle = new QAction("Auto Save", mainWindow);
    menuActionAutoSaveToggle->setCheckable(true);
    menuFile->addAction(menuActionAutoSaveToggle);

    menuActionChangeAutoSaveDir = new QAction("Change Auto-Save Directory...", mainWindow);
    menuFile->addAction(menuActionChangeAutoSaveDir);

    menuFile->addSeparator();

    menuActionSaveFrame = menuActionSaveFrameAutoNumber;
    actionSaveFrame = menuActionSaveFrameAutoNumber;

    QMenu *menuCamera = menuBar->addMenu("&Camera");
    menuActionConfig = new QAction("&Config", mainWindow);
    menuActionConfig->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    menuActionConfig->setShortcutContext(Qt::ApplicationShortcut);
    menuCamera->addAction(menuActionConfig);
    actionConfig = menuActionConfig;

    QMenu *menuAnalyse = menuBar->addMenu("&Analyse");
    menuActionStatistics = new QAction("&Statistics", mainWindow);
    menuActionStatistics->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    menuAnalyse->addAction(menuActionStatistics);

    QMenu *menuView = menuBar->addMenu("&View");
    menuActionColorScale = new QAction("Color &Scale", mainWindow);
    QMenu *subMenuColorScale = new QMenu(mainWindow);

    menuActionColorScaleAuto = new QAction("&Auto (Image Range)", mainWindow);
    menuActionColorScaleAuto->setCheckable(true);
    subMenuColorScale->addAction(menuActionColorScaleAuto);

    menuActionColorScale8Bit = new QAction("&8-bit (0-255)", mainWindow);
    menuActionColorScale8Bit->setCheckable(true);
    subMenuColorScale->addAction(menuActionColorScale8Bit);

    menuActionColorScale16Bit = new QAction("&16-bit (0-65535)", mainWindow);
    menuActionColorScale16Bit->setCheckable(true);
    subMenuColorScale->addAction(menuActionColorScale16Bit);

    menuActionColorScale->setMenu(subMenuColorScale);
    menuView->addAction(menuActionColorScale);

    menuActionSpectrumRange = new QAction("&Spectrum Range", mainWindow);
    QMenu *subMenuSpectrumRange = new QMenu(mainWindow);

    menuActionSpectrumRangeAuto = new QAction("&Auto (Fit All)", mainWindow);
    menuActionSpectrumRangeAuto->setCheckable(true);
    subMenuSpectrumRange->addAction(menuActionSpectrumRangeAuto);

    menuActionSpectrumRangeFull = new QAction("&Full (Show All)", mainWindow);
    menuActionSpectrumRangeFull->setCheckable(true);
    subMenuSpectrumRange->addAction(menuActionSpectrumRangeFull);

    menuActionSpectrumRangeZoomLeft = new QAction("Zoom &Left (0-50%)", mainWindow);
    menuActionSpectrumRangeZoomLeft->setCheckable(true);
    subMenuSpectrumRange->addAction(menuActionSpectrumRangeZoomLeft);

    menuActionSpectrumRangeZoomRight = new QAction("Zoom &Right (50-100%)", mainWindow);
    menuActionSpectrumRangeZoomRight->setCheckable(true);
    subMenuSpectrumRange->addAction(menuActionSpectrumRangeZoomRight);

    menuActionSpectrumRangeZoomCenter = new QAction("Zoom &Center (25-75%)", mainWindow);
    menuActionSpectrumRangeZoomCenter->setCheckable(true);
    subMenuSpectrumRange->addAction(menuActionSpectrumRangeZoomCenter);

    menuActionSpectrumRangeCustom = new QAction("&Custom...", mainWindow);
    menuActionSpectrumRangeCustom->setCheckable(true);
    subMenuSpectrumRange->addAction(menuActionSpectrumRangeCustom);

    menuActionSpectrumRange->setMenu(subMenuSpectrumRange);
    menuView->addAction(menuActionSpectrumRange);

    menuActionShowAxes = new QAction("Show Image Axes", mainWindow);
    menuActionShowAxes->setCheckable(true);
    menuActionShowAxes->setChecked(false);
    menuView->addAction(menuActionShowAxes);

    QMenu *menuPostProcess = menuBar->addMenu("&Post-Process");
    menuActionVerticalBinning = new QAction("Software Vertical Binning", mainWindow);
    menuActionVerticalBinning->setShortcut(QKeySequence(Qt::Key_V));
    menuActionVerticalBinning->setShortcutContext(Qt::ApplicationShortcut);
    menuActionVerticalBinning->setCheckable(true);
    menuPostProcess->addAction(menuActionVerticalBinning);

    menuActionRowRange = new QAction("Row Range...", mainWindow);
    menuPostProcess->addAction(menuActionRowRange);
    menuActionPostProcess = menuPostProcess->menuAction();

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