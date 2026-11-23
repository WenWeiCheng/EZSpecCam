# src/gui — Qt GUI Application

**Generated:** 2026-05-17
**Updated:** 2026-11-23 (single-file dialogs; services/; layout-only MainWindowUi)

## OVERVIEW
Qt 6 GUI app with QCustomPlot-based image/spectrum display, camera parameter configuration, and plugin loading. Entry point: `main.cpp` → `QApplication` → `MainWindow`.

The GUI is split into four layers with a one-way dependency direction:

```
main.cpp / AppController / MessageHandler / Theme
        │
        ▼
widgets/ (windows + interactive widgets)
   ├── dialogs/     self-contained dialogs (camera config, calibration, editors)
   └── services/    workflow helpers + pure algorithms (no QtWidgets)
        │
        ▼
core/ (camera interface, calibration, image types)
```

- `ui/` contains layout-only helpers. Today that is `MainWindowUi` only.
- `widgets/` owns its dialogs/windows, wires signals and calls services.
- `services/` must not include QtWidgets. It may use QtCore/QtGui and `core/`.
- `core/` must stay GUI-independent.

## STRUCTURE
```
gui/
├── main.cpp                    # QApplication entry
├── AppController.cpp/h         # State machine + plugin manager (merged)
├── DebugMacros.h               # QDEBUG_* macros
├── Theme.cpp/h                 # Light/dark palette + QCustomPlot colours
├── qcustomplot.cpp/h           # EXTERNAL — DO NOT MODIFY
├── ui/
│   └── MainWindowUi            # Layout-only: menus, toolbar, status bar, stack
├── widgets/
│   ├── MainWindow.cpp/h        # Main shell: window assembly + signal wiring
│   ├── common/                 # Reusable atoms, not camera-specific
│   │   ├── LoadingIndicator
│   │   └── ParameterWidgetFactory
│   ├── display/                # Image/curve/histogram widgets + display windows
│   │   ├── ImageViewWidget     # QCustomPlot-based frame rendering
│   │   ├── LineViewWidget      # Generic x-y curve plot
│   │   ├── HistogramViewWidget # Bar-chart plot (histogram only)
│   │   ├── HistogramWindow     # Histogram + overexposure statistics
│   │   ├── ProfileWindow       # Cross-section profile
│   │   ├── ParameterHistoryWindow
│   │   └── StatisticsDialog
│   └── dialogs/                # Top-level dialogs, each self-contained in one .cpp
│       ├── CameraConfigDialog  # Camera / Data / Plugins pages + telemetry panel
│       ├── CalibrationDialog
│       ├── DespikeDialog
│       ├── RowRangeDialog
│       ├── ScaleControlDialog
│       └── DisplayStyleDialog
├── services/                   # No QtWidgets; unit-testable
│   ├── AppSettings             # ONLY place with application QSettings keys
│   ├── FileNaming              # Frame/reference file-name builders
│   ├── HistogramBinner         # Binning + overexposure count
│   ├── HistoryCsvExporter      # Telemetry history CSV writer
│   ├── ImageStatistics         # min/max/avg/stddev for image or spectrum
│   ├── ParameterHistoryStore   # Sliding-window telemetry model
│   └── PostProcess             # vertical binning, dark calibration, despike
└── workers/                    # File loader/saver worker objects
```

## DIALOG CONVENTIONS
- **Dialogs are single-file by default.** A dialog's `.h` exposes only the
  dialog class; its `.cpp` contains `setupUi()` and the page/helper functions.
  `CameraConfigDialog` is the reference example: `setupUi()` calls
  `setupCameraPage()`, `setupDataPage()` and `setupPluginsPage()`.
- **Do not reintroduce per-tab classes.** `CameraConfigDialog` deliberately has
  no `CameraTab`, `DataTab` or `PluginTab` classes and no `CameraTabUi` /
  `DataTabUi` / `CameraConfigDialogUi`. If a page becomes reusable by another
  window, extract a widget then, not pre-emptively.
- `widgets/dialogs/` holds every top-level dialog. There is no `config/`
  directory. If you need shared state, put it in `services/`; if you need a
  reusable control, put it in `widgets/common/`.
- Keep page construction separated by function (`setupCameraPage`,
  `setupDataPage`, ...), not by class or file. This keeps a large dialog
  navigable without scattering it across directories.
- **The owner creates child windows.** `MainWindow` creates
  `ImageViewWidget`/`LineViewWidget` and passes them to `MainWindowUi`;
  `MainWindowUi` does not `new` project widgets. A dialog may build its own
  private pages, because those pages are not shared with another window.
- **`MainWindowUi` is layout-only.** No `QSettings`, `QFileDialog`,
  `QMessageBox`, `AppController`, or business-signal connections there. The
  owner wires accepted/rejected/restore and forwards the controller.
- `ui` members are private. Expose intent methods (`showLoading`,
  `hideLoading`, `showParameterPanel`) or narrow accessors; never expose a
  `getUi()` or reach through `dialog->getUi()->tab->ui->widget`.
- A widget's `objectName` is the stable hook for tests. Set it when a test needs
  to find the control; do not make an internal widget table public for tests.
- New UI code must not include `widgets/display` or `widgets/dialogs` from
  `ui/`. Put shared atoms in `widgets/common/` instead.

## SERVICES & SETTINGS
- `services/` is for things that can be tested without a `QWidget`:
  algorithms, file formats, naming, settings, state models.
- `services/` must not include QtWidgets. Report failures with
  `bool` + `QString *error` or a result struct; let the view show the dialog.
- Application preferences go through `AppSettings` only. Do not construct
  `QSettings` in a widget or dialog. `AppController` is the one exception: its
  per-camera INI file is driver configuration, not application preferences.
- Pure services have a dedicated test target (`tests/test_gui_services`) that
  links without QCustomPlot. Add a test there when you add a pure function.
- `PostProcess` is an image algorithm, not a widget: keep it in `services/`.
  Do not move it back under `widgets/`.

## WHERE TO LOOK
| Task | Location |
|------|----------|
| App startup + state machine | `AppController.cpp` |
| Main window + toolbar actions | `widgets/MainWindow.cpp` |
| Main window layout | `ui/MainWindowUi.cpp` |
| Camera / Data / Plugins dialog | `widgets/dialogs/CameraConfigDialog.cpp` |
| Frame rendering | `widgets/display/ImageViewWidget.cpp` |
| Curve plots (spectrum / profile) | `widgets/display/LineViewWidget.cpp` |
| Telemetry history (record + plot + CSV) | `services/ParameterHistoryStore.cpp`, `widgets/display/ParameterHistoryWindow.cpp` |
| Histogram + overexposure count | `services/HistogramBinner.cpp`, `widgets/display/HistogramWindow.cpp` |
| Image/spectrum statistics | `services/ImageStatistics.cpp`, `widgets/display/StatisticsDialog.cpp` |
| Despike algorithm | `services/PostProcess.cpp` |
| Despike pipeline (calibration → despike → vbin) | `widgets/MainWindow.cpp` (`rebuildDespikeAndBinning`) |
| Dynamic parameter widgets | `widgets/common/ParameterWidgetFactory.cpp` |
| Application settings keys | `services/AppSettings.cpp` |
| Saved-file naming | `services/FileNaming.cpp` |
| Colours (GUI chrome + plots) | `Theme.cpp` |

## CONVENTIONS
- `Q_OBJECT` macro always present in QObject subclasses
- `signals:` / `slots:` sections follow Qt naming (not `Q_SIGNALS`/`Q_SLOTS`)
- New slots use camelCase (`onCaptureModeChanged`). Older Designer-style
  `on_<object>_<signal>` names are kept as-is; do not mass-rename them.
- Members use the `m_` prefix. Public fields are acceptable only inside a
  layout-only `*Ui` class, where they are the widget table.
- Prefer enum-typed signals over `int` codes in new code.
- Forward-declare widget pointers in headers; include the concrete Qt widget
  only in the `.cpp`.
- Never hardcode a colour in a widget. Take it from `Theme::plotColors()`
- QCustomPlot ignores `QWidget` palettes entirely, so plot colours must be set
  explicitly. New plots go through `Theme::applyToPlot()` rather than being
  styled in place, or they will stay light in dark mode
- Camera state machine: Disconnected → Connecting → Connected → Acquiring → Error
- Telemetry polling (temperature/history) keeps running while `Acquiring`:
  `CameraConfigDialog` gates it on `AppController::isActive()` (Connected **or**
  Acquiring), not `isConnected()`, otherwise the curves stop as soon as a
  capture starts. Parameter writes still require `isConnected()` (idle)
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
  `m_vBinEndRow` (1-based, `-1` = last row), independent of whether vbin is on.
  `RowRangeDialog` is 1-based too, so `on_rowRange_triggered` passes the
  spin-box values straight through — do not add ±1 conversions there.
  Rows outside it are copied unchanged and never despiked
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

## KNOWN DEBT / NEXT MIGRATIONS
These are the remaining places where the layer boundary is still crossed. Do
not add more logic there; extract it when you touch the file.

- `MainWindow` still owns the calibration state machine, reference-frame
  burst accumulation, frame pipeline and save orchestration. Target:
  `services/CalibrationSession`, `services/FramePipeline`,
  `services/FrameSaveController`; `MainWindow` keeps window wiring only.
- `CameraConfigDialog.cpp` is intentionally large (camera + data + plugins +
  telemetry). Keep it navigable with `setup*Page()` sections. Only split a page
  into a reusable widget when another window actually needs it.
- `CalibrationDialog` also builds its layout inline; keep it single-file unless
  it grows a reusable page. Use private `setup*Page()` helpers there too.
- Display dialogs still emit `int` codes (`ColorMap`, `LineStyle`, scale modes).
  Target: shared enums in `gui/types/` and enum-typed signals.
- `MainWindow.cpp` exceeds the target size budget. Keep new behavior out of it;
  see the split targets above.

## ANTI-PATTERNS
- **NEVER** modify `qcustomplot.cpp` or `qcustomplot.h` — external library
- **NEVER** block the main thread in signal handlers
- **NEVER** put `QSettings`, `QFileDialog`, `QMessageBox` or `AppController`
  into a layout-only `*Ui`, and never `new` a project view from `MainWindowUi`
- **NEVER** reach through a `*Ui` from another class (`getUi()->...`); add an
  intent method or a narrow accessor on the owning widget
- **NEVER** add algorithms or file/CSV formats to a dialog; put them in
  `services/` and call them from the dialog
- **NEVER** reintroduce `CameraTab` / `DataTab` / `PluginTab` classes or a
  `widgets/config/` directory; keep the camera dialog single-file
- **NEVER** link QCustomPlot into a pure-service test just to test an algorithm
- Plugin scanning is synchronous at startup — could be slow with many plugins
