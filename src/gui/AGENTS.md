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
    │   ├── SpectrumViewWidget   # Spectrum plot
    │   ├── ProfileWindow        # Cross-section profile
    │   └── StatisticsDialog     # Frame statistics
    └── dialogs/                 # Utility dialogs
        ├── CustomRangeDialog    # Custom spectrum range input
        └── RowRangeDialog       # Row binning range input
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| App startup + state machine | `AppController.cpp` |
| Main window + toolbar actions | `widgets/MainWindow.cpp` |
| UI layout construction | `ui/MainWindowUi.cpp` |
| Frame rendering | `widgets/display/ImageViewWidget.cpp` |
| Spectrum display | `widgets/display/SpectrumViewWidget.cpp` |
| Dynamic parameter widgets | `widgets/config/ParameterWidgetFactory.cpp` |

## CONVENTIONS
- `Q_OBJECT` macro always present in QObject subclasses
- `signals:` / `slots:` sections follow Qt naming (not `Q_SIGNALS`/`Q_SLOTS`)
- UI construction separated into `ui/MainWindowUi` (not in MainWindow itself)
- Camera state machine: Disconnected → Connecting → Connected → Acquiring → Error
- Frame delivery via `frameReady(const ImageData &)` signal (not polling)
- Functions defined with slots in `AppController` must be invoked via `QMetaObject::invokeMethod()`.

## ANTI-PATTERNS
- **NEVER** modify `qcustomplot.cpp` or `qcustomplot.h` — external library
- **NEVER** block the main thread in signal handlers
- Plugin scanning is synchronous at startup — could be slow with many plugins
