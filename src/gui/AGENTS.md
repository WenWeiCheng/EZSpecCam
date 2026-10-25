# src/gui — Qt GUI Application

**Generated:** 2026-05-17

## OVERVIEW
Qt 6.8 GUI app with QCustomPlot-based image/spectrum display, camera parameter configuration, and plugin loading. Entry point: `main.cpp` → `QApplication` → `MainWindow`.

## STRUCTURE
```
gui/
├── main.cpp                    # QApplication entry
├── AppController.cpp/h         # State machine + plugin manager (merged)
├── DebugMacros.h               # QDEBUG_* macros
├── Theme.cpp/h                 # Light/dark palette + QCustomPlot colours
├── qcustomplot.cpp/h           # EXTERNAL — DO NOT MODIFY
├── ui/
│   └── MainWindowUi.cpp/h      # Separated UI construction
└── widgets/
    ├── MainWindow.cpp/h         # Main QMainWindow
    ├── config/                  # Camera parameter configuration UI
    │   ├── CameraTab            # Exposure/gain/offset controls
    │   ├── DataTab              # Data acquisition settings
    │   ├── PluginTab            # Plugin selection
    │   ├── ParameterWidgetFactory  # Dynamic widget creation from ParameterDefinition
    │   ├── CameraConfigDialog   # Full camera settings dialog
    │   └── LoadingIndicator     # Spinner overlay
    ├── display/                 # Image/spectrum display widgets
    │   ├── ImageViewWidget      # QCustomPlot-based frame rendering
    │   ├── LineViewWidget       # Generic x-y curve plot
    │   ├── ProfileWindow        # Cross-section profile
    │   ├── HistogramViewWidget  # Bar-chart plot (histogram only)
    │   ├── HistogramWindow      # Histogram + overexposure statistics
    │   └── StatisticsDialog     # Frame statistics
    ├── PostProcess.cpp/h        # Algorithms: vertical binning, legacy dark calibration, image despike
    └── dialogs/                 # Utility dialogs
        ├── CustomRangeDialog    # Custom spectrum range input
        ├── RowRangeDialog       # Row range shared by binning and despike
        ├── CalibrationDialog    # Dark/Flat tabs: acquire, load or Save As references; flat reuses the dark model
        └── DespikeDialog        # Despike primary threshold (single parameter)
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| App startup + state machine | `AppController.cpp` |
| Main window + toolbar actions | `widgets/MainWindow.cpp` |
| UI layout construction | `ui/MainWindowUi.cpp` |
| Frame rendering | `widgets/display/ImageViewWidget.cpp` |
| Curve plots (spectrum / profile) | `widgets/display/LineViewWidget.cpp` |
| Histogram + overexposure count | `widgets/display/HistogramWindow.cpp` |
| Despike (algorithm + pipeline + params window) | `widgets/PostProcess.cpp`, `widgets/MainWindow.cpp` (`rebuildDespikeAndBinning`), `widgets/dialogs/DespikeDialog.cpp` |
| Dynamic parameter widgets | `widgets/config/ParameterWidgetFactory.cpp` |
| Colours (GUI chrome + plots) | `Theme.cpp` |

## CONVENTIONS
- `Q_OBJECT` macro always present in QObject subclasses
- `signals:` / `slots:` sections follow Qt naming (not `Q_SIGNALS`/`Q_SLOTS`)
- UI construction separated into `ui/MainWindowUi` (not in MainWindow itself)
- Never hardcode a colour in a widget. Take it from `Theme::plotColors()`
- QCustomPlot ignores `QWidget` palettes entirely, so plot colours must be set
  explicitly. New plots go through `Theme::applyToPlot()` rather than being
  styled in place, or they will stay light in dark mode
- Camera state machine: Disconnected → Connecting → Connected → Acquiring → Error
- `QCPBars` cannot draw a bar whose base is 0 on a logarithmic axis —
  `QCPBars::getBarRect` computes `coordToPixel(base + value)` and `log10(0)`
  is `-inf`, which destroys the bar geometry. `HistogramViewWidget` therefore
  moves the bar base up to the axis lower bound while the log scale is on
  (`kLogBase`), which also makes zero-count bins draw as nothing
- Despike is part of the acquisition pipeline, not the display widgets:
  `MainWindow::rebuildDespikeAndBinning()` takes `m_preDespikeFrame` (the frame
  after calibration), runs `PostProcess::despikeImage()`, then optional
  `verticalBinning()`, and stores the result in `m_currentFrame`. The spectrum
  view therefore sees a spectrum binned from despiked data, and saving/
  auto-save writes the same processed frame. Toggling despike, changing the
  threshold or changing the row range rebuilds from `m_preDespikeFrame`
- Both views share one Process menu toggle. The row range is `m_vBinStartRow` /
  `m_vBinEndRow` (1-based, `-1` = last row), independent of whether vbin is on;
  rows outside it are copied unchanged and never despiked
- With vbin + a partial row range, the live pipeline only despikes the ROI and
  leaves `m_currentFrame.originalImage` as the raw 2D frame. `saveFrameToFile()`
  rebuilds the full despiked 2D with the same row range before handing the
  request to `FileSaverWorker`, so saved files always contain the despike result.
  This is covered by `test_despike_pipeline`'s save case
- `DespikeParams` has a single field, `primaryThreshold`. `despikeImage()`
  removes pixels strictly higher than all 8 neighbours by more than that many
  per-row noise sigmas, replacing them with the median of the 4 direct
  neighbours. There is no longer a 1D spectrum variant
- `despikeImage()` allocates the output and writes every pixel in the same
  OpenMP pass (it does not `QImage::copy()` first). That halves the memory
  traffic; the two-pass per-row max keeps the hot loop branch-free and
  vectorizable. On an i5-12500H (16 threads) in Release, 6000×4000 Grayscale16
  measures ~60–70 ms (~15–17 fps) for the full image. With vbin + a 100-row
  range the pipeline copies only the ROI (plus one halo row each side) and bins
  that, so it runs in ~20–30 ms (~35–50 fps) and defers the full 2D output to
  save time
- `verticalBinning()` reads the source with `constBits()`/`constScanLine()`.
  It must not call the non-const `bits()`: `image` and `originalImage` usually
  share the same buffer, and detaching would copy the whole frame (~50 ms at
  6000×4000) just to read it
- Frame delivery via `frameReady(const ImageData &)` signal (not polling)
- Functions defined with slots in `AppController` must be invoked via `QMetaObject::invokeMethod()`.

## ANTI-PATTERNS
- **NEVER** modify `qcustomplot.cpp` or `qcustomplot.h` — external library
- **NEVER** block the main thread in signal handlers
- Plugin scanning is synchronous at startup — could be slow with many plugins
