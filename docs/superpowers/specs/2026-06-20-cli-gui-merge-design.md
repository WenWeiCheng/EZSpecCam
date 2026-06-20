# CLI/GUI Single-Binary Merge — Design Spec

**Date:** 2026-06-20
**Scope:** Collapse `ezspeccam_cli.exe` and `ezspeccam_gui.exe` into one `ezspeccam.exe` with a new `src/app/` shared layer. No new features; this is a refactor.

## Goal

- Single executable `ezspeccam.exe`; mode (headless / windowed) decided at runtime by argv.
- One shared code layer `src/app/` reused by both modes — eliminates duplicated plugin loading, frame writing, wait-stabilize, and sequence running.
- Build matrix simplified: one CMake target, one build flag (`EZSPECCAM_BUILD_APP`).
- No user-visible CLI flag changes. The only externally observable change is the binary name (`ezspeccam_cli.exe` → `ezspeccam.exe`) and the unified CSV / metadata format described in §Behavioural Decisions.

## Behavioural Decisions (locked)

| Dimension | Decision |
|---|---|
| Mode dispatch | No args → Windowed (GUI). Any of `--list` / `--list-params` / `--camera` / `--set` / `--frames` / `--sequence` / `--output` / `--format` / `--prefix` / `--suffix` / `--help` / `--version` → Headless. |
| Windows console | Binary subsystem `WIN32`; headless mode calls `AttachConsole(ATTACH_PARENT_PROCESS)`, `freopen`s stdout/stderr to `CONOUT$`. GUI mode does not attach. |
| Build flag | `EZSPECCAM_BUILD_GUI` + `EZSPECCAM_BUILD_CLI` → single `EZSPECCAM_BUILD_APP` (default `ON`). |
| Binary name | `ezspeccam.exe`. |
| Plugin search roots | `<appdir>/plugins/drivers` AND `<appdir>/../plugins/drivers` (CLI's permissive set wins over GUI's single-root). |
| CSV format | **Unified to wide format** (no header, one line per image row, values comma-separated). CLI's previous long format (`Row,Col,Value` + one line per pixel) is dropped. **Breaking change** for any script parsing the old CLI CSV. |
| Metadata JSON schema | **Always includes `softwareSettings`** (empty `{}` when no software vbin). CLI's previous schema (no `softwareSettings` key) is dropped. |
| `app::plugins::Entry` shape | Public `Entry { QString filePath; QStringList cameraIds; ICameraDriver *instance; }` — `filePath` is required because `PluginTab` displays the DLL file name. |
| `PluginLoader` thread model | Stateless namespace + static methods. AppController calls them on its own QThread. No global mutable state. |
| `tests/test_cli/` | Deleted (already disabled in `tests/CMakeLists.txt`; references nonexistent `cli/CommandLineParser.h` and `cli/CaptureController.h`). |

## Target Directory Layout

```
src/
├── core/                              # unchanged
├── app/                               # NEW shared layer
│   ├── CMakeLists.txt                 # single qt_add_executable(ezspeccam WIN32)
│   ├── main.cpp                       # entry: parseAppMode → QApplication or QCoreApplication branch
│   ├── AppMode.h                      # enum class app::Mode { Headless, Windowed }
│   ├── AppMode.cpp                    # app::parseAppMode(int argc, char**)
│   ├── MessageHandler.h/.cpp          # qInstallMessageHandler + Windows AttachConsole
│   ├── PluginLoader.h/.cpp            # app::plugins namespace: scan, entries, findByCamera, enumerateCameras
│   ├── HeadlessController.h/.cpp      # app::runHeadless(HeadlessOptions) — the pipeline
│   ├── WaitStabilizer.h/.cpp          # app::waitForStable(driver, name, target, tol, winSec, timeout)
│   ├── SequenceRunner.h/.cpp          # MOVED from src/cli/ unchanged
│   └── formats/                       # MOVED from src/gui/workers/{formats/,SaveTypes.h,IImageFormatHandler.h}
│       ├── IImageFormatHandler.h
│       ├── SaveTypes.h
│       ├── TiffFormatHandler.h/.cpp
│       └── CsvFormatHandler.h/.cpp
├── cli/
│   ├── CliOptions.h/.cpp              # QCommandLineParser → cli::Options struct
│   └── CliMain.cpp                    # cli::run(int argc, char**, QCoreApplication&)
├── gui/
│   ├── GuiMain.cpp                    # gui::run(QApplication&) → AppController + MainWindow
│   ├── AppController.h/.cpp           # refactored — uses app::plugins::scan()
│   ├── DebugMacros.h                  # unchanged
│   ├── qcustomplot.cpp/h              # unchanged
│   ├── widgets/                       # unchanged
│   └── workers/                       # FileSaverWorker keeps its slot/dispatch role; delegates I/O to app::formats
│       ├── FileSaverWorker.h/.cpp
│       └── FileLoaderWorker.h/.cpp    # moved out? → see §Open Question 1
├── plugins/
│   └── mock/CMakeLists.txt            # POST_BUILD deploy target renamed ezspeccam_gui → ezspeccam
└── tests/                             # paths updated; no new tests required, but test_cli/ removed
```

## Component Contracts

### `app::parseAppMode` (`src/app/AppMode.cpp`)

```cpp
namespace app {
    enum class Mode { Headless, Windowed };
    Mode parseAppMode(int argc, char *argv[]);
}
```

Recognised headless triggers: any of `--list`, `--list-params`, `--camera`, `--set`, `--frames`, `--sequence`, `--output`, `--format`, `--prefix`, `--suffix`, `--help`, `--version`. Anything else → `Windowed`.

### `app::MessageHandler` (`src/app/MessageHandler.cpp`)

- `void installMessageHandler()` — registers `qInstallMessageHandler` callback routing `qInfo`/`qWarning`/`qDebug` to stdout, `qCritical`/`qFatal` to stderr.
- `void attachParentConsoleIfAvailable()` — Windows: `AttachConsole(ATTACH_PARENT_PROCESS)`, then `freopen("CONOUT$", "w", stdout)` / `stderr`. On non-Windows, no-op.

### `app::plugins` namespace (`src/app/PluginLoader.cpp`)

```cpp
namespace app::plugins {
    struct Entry { QString filePath; QStringList cameraIds; ICameraDriver *instance; };

    // Scan the two default roots; load each DLL; populate the global registry.
    // Idempotent — clears existing registry first.
    int scanDefaultRoots();             // returns number of plugins loaded
    void setExtraRoots(const QStringList &roots);  // for tests

    const QVector<Entry> &entries();
    const Entry *findByCamera(const QString &cameraId);
    QStringList enumerateCameras();
    void unloadAll();
}
```

- The registry is process-global but accessed only from a single thread (AppController's thread, or the CLI's main thread).
- Default roots: `QCoreApplication::applicationDirPath() + "/plugins/drivers"` and `+ "/../plugins/drivers"`.
- `QPluginLoader` instances are owned by the registry; unloaded in `unloadAll()`.

### `app::formats` namespace (`src/app/formats/`)

`IImageFormatHandler` interface and `TiffFormatHandler` / `CsvFormatHandler` are moved here **unchanged** except for the CSV handler becoming the canonical implementation (which it already was — the GUI version writes wide format). Metadata JSON in both handlers now always includes `softwareSettings` (the GUI version already does — confirmed in `tests/test_metadata_save_load`).

Add a thin top-level dispatcher:

```cpp
namespace app::formats {
    bool saveFrame(const ImageData &frame, const QString &filePath);  // dispatch by extension
    QStringList supportedSaveExtensions();
    QString generateFilename(const QString &outputDir, const QString &prefix,
                             const QString &suffix, const QString &extension);
    QString extensionForCliFormat(const QString &cliFormat);  // "tiff" → "tiff", "csv" → "csv"
}
```

The `extension` parameter on `generateFilename` keeps the CLI's `--format` flag working: `cli::Options` carries the user's format string, then `cli::CliMain` calls `extensionForCliFormat` and threads the result through. Unknown `cliFormat` values fall back to `"tiff"` (with a `qWarning`), preserving current CLI behaviour.

### `app::HeadlessController` (`src/app/HeadlessController.cpp`)

```cpp
namespace app {
    struct HeadlessOptions {
        bool listCameras = false;
        bool listParams = false;
        QString cameraId;
        QVariantMap setParameters;
        int frames = 1;
        QString outputDir = ".";
        QString prefix;
        QString suffix;
        QVector<SequenceStep> sequence;  // populated by cli::Options when --sequence used
    };
    int run(const HeadlessOptions &opts);  // 0 success, 1 user error, -1 internal
}
```

The pipeline is extracted from `src/cli/main.cpp:228-310` and `422-478`. Frame saving goes through `app::formats::saveFrame`. `waitForStable` (now in `app::WaitStabilizer`) is called from the sequence path. The frame's `softwareSettings` is left empty (CLI semantic preserved).

### `cli::run` (`src/cli/CliMain.cpp`)

Builds a `cli::Options` via `QCommandLineParser`, then maps it to `app::HeadlessOptions` and calls `app::run(opts)`.

### `gui::run` (`src/gui/GuiMain.cpp`)

Constructs `AppController` (on the QThread managed by `MainWindow`), constructs `MainWindow`, calls `window.show()`. Surface format setup is preserved from the current `src/gui/main.cpp:16-21`.

## AppController Refactor

Current `AppController` (`src/gui/AppController.h/.cpp`) owns:
- `QList<PluginInfo> m_plugins` — **removed**. Replaced by calls to `app::plugins::entries()`.
- `struct PluginInfo` — **removed**. Callers (`PluginTab`) include `app/PluginLoader.h` and use `app::plugins::Entry` directly.
- `void scanPlugins()` slot — **kept as a thin wrapper** that calls `app::plugins::scanDefaultRoots()` and emits the same `pluginScanCompleted` / `pluginScanProgress` / `pluginLoadFailed` signals. Signal payload for `pluginLoadFailed` synthesises from per-file try/catch in the loader.
- `QList<PluginInfo> loadedPlugins() const` — **replaced by** `QVector<app::plugins::Entry> loadedPlugins() const` that returns `app::plugins::entries()`.
- `const PluginInfo *findPluginForCamera(const QString &)` — **removed**; `connectCamera` calls `app::plugins::findByCamera(cameraId)`.
- `bool loadPlugin(const QString &)` / `void unloadPlugin(const PluginInfo &)` / `void clearPlugins()` — **all removed** (logic moved to `PluginLoader`).
- `QString m_pluginDir` / `setPluginDirectory` — **removed**; `PluginTab` no longer overrides root paths. (If a custom path is ever needed, it goes through `app::plugins::setExtraRoots` — not exposed in UI for now.)

Everything else (state machine, INI persistence, parameter cache, frame routing) is unchanged.

`tests/test_app_controller/CMakeLists.txt` already includes `src/gui/AppController.cpp` directly — only `AppController.h` needs to add `#include "app/PluginLoader.h"`.

## File Migration Map

| Old | New |
|---|---|
| `src/cli/main.cpp:1-122` (TIFF/CSV writers + metadata + dispatch) | `src/app/HeadlessController.cpp` + `src/app/formats/TiffFormatHandler.cpp` (existing) + `src/app/formats/CsvFormatHandler.cpp` (existing) |
| `src/cli/main.cpp:130-170` (`loadAllDrivers`) | `src/app/PluginLoader.cpp` |
| `src/cli/main.cpp:176-201` (`findCamera`, `listCameras`) | `src/app/HeadlessController.cpp` (`listCameras` becomes part of `app::run` with `listCameras=true`) |
| `src/cli/main.cpp:228-311` (`captureFrames`) | `src/app/HeadlessController.cpp` |
| `src/cli/main.cpp:317-416` (`waitForStable`) | `src/app/WaitStabilizer.cpp` |
| `src/cli/main.cpp:422-478` (`runSequence`) | `src/app/HeadlessController.cpp` (calls `app::WaitStabilizer::wait`) |
| `src/cli/main.cpp:484-489` (`messageHandler`) | `src/app/MessageHandler.cpp` |
| `src/cli/main.cpp:491-666` (`QCommandLineParser` + `main`) | `src/cli/CliOptions.cpp` (parsing) + `src/cli/CliMain.cpp` (entry) + `src/app/main.cpp` (mode dispatch) |
| `src/cli/SequenceRunner.{h,cpp}` | `src/app/SequenceRunner.{h,cpp}` (unchanged contents) |
| `src/gui/main.cpp:1-27` | `src/app/main.cpp` (mode branch) + `src/gui/GuiMain.cpp` (`gui::run`) |
| `src/gui/workers/SaveTypes.h` | `src/app/formats/SaveTypes.h` |
| `src/gui/workers/IImageFormatHandler.h` | `src/app/formats/IImageFormatHandler.h` |
| `src/gui/workers/formats/TiffFormatHandler.{h,cpp}` | `src/app/formats/TiffFormatHandler.{h,cpp}` |
| `src/gui/workers/formats/CsvFormatHandler.{h,cpp}` | `src/app/formats/CsvFormatHandler.{h,cpp}` |
| `src/gui/workers/FileLoaderWorker.{h,cpp}` | **stays in `src/gui/workers/`** — GUI-only (Open File dialog); CLI never reads |
| `src/cli/CMakeLists.txt` | deleted (folded into `src/app/CMakeLists.txt`) |
| `src/gui/CMakeLists.txt` | deleted (folded into `src/app/CMakeLists.txt`) |

## CMake Changes

### Root `CMakeLists.txt`

- Remove `option(EZSPECCAM_BUILD_GUI …)`, `option(EZSPECCAM_BUILD_CLI …)`.
- Add `option(EZSPECCAM_BUILD_APP "Build the unified application" ON)`.
- Replace `if(EZSPECCAM_BUILD_GUI) add_subdirectory(src/gui) endif()` and the CLI equivalent with a single `if(EZSPECCAM_BUILD_APP) add_subdirectory(src/app) endif()`.
- Update summary message.

### `CMakePresets.json` (3 presets)

Each `cacheVariables` block: remove `EZSPECCAM_BUILD_GUI`, remove `EZSPECCAM_BUILD_CLI`, add `EZSPECCAM_BUILD_APP: "ON"`. The `msvc-debug-gui` preset loses its distinguishing flag — keep it as an alias that still produces the same binary; the build matrix collapses to one shape.

### New `src/app/CMakeLists.txt`

```cmake
qt_add_executable(ezspeccam WIN32)
target_link_libraries(ezspeccam PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::OpenGL
    OpenMP::OpenMP_CXX
    OpenGL::GL)
target_compile_definitions(ezspeccam PRIVATE QCUSTOMPLOT_USE_OPENGL)
target_sources(ezspeccam PRIVATE
    main.cpp AppMode.h AppMode.cpp
    MessageHandler.h MessageHandler.cpp
    PluginLoader.h PluginLoader.cpp
    HeadlessController.h HeadlessController.cpp
    WaitStabilizer.h WaitStabilizer.cpp
    SequenceRunner.h SequenceRunner.cpp
    formats/IImageFormatHandler.h formats/SaveTypes.h
    formats/TiffFormatHandler.h formats/TiffFormatHandler.cpp
    formats/CsvFormatHandler.h formats/CsvFormatHandler.cpp
    cli/CliOptions.h cli/CliOptions.cpp cli/CliMain.cpp
    gui/GuiMain.cpp
    gui/DebugMacros.h
    gui/AppController.h gui/AppController.cpp
    gui/qcustomplot.h gui/qcustomplot.cpp
    gui/widgets/MainWindow.h gui/widgets/MainWindow.cpp
    gui/widgets/PostProcess.h gui/widgets/PostProcess.cpp
    gui/ui/MainWindowUi.h gui/ui/MainWindowUi.cpp
    gui/widgets/config/CameraTab.h gui/widgets/config/CameraTab.cpp
    gui/ui/CameraTabUi.h gui/ui/CameraTabUi.cpp
    gui/widgets/config/LoadingIndicator.h gui/widgets/config/LoadingIndicator.cpp
    gui/widgets/config/ParameterWidgetFactory.h gui/widgets/config/ParameterWidgetFactory.cpp
    gui/widgets/config/DataTab.h gui/widgets/config/DataTab.cpp
    gui/ui/DataTabUi.h gui/ui/DataTabUi.cpp
    gui/widgets/config/PluginTab.h gui/widgets/config/PluginTab.cpp
    gui/widgets/config/CameraConfigDialog.h gui/widgets/config/CameraConfigDialog.cpp
    gui/ui/CameraConfigDialogUi.h gui/ui/CameraConfigDialogUi.cpp
    gui/widgets/display/ImageViewWidget.h gui/widgets/display/ImageViewWidget.cpp
    gui/widgets/display/SpectrumViewWidget.h gui/widgets/display/SpectrumViewWidget.cpp
    gui/widgets/display/StatisticsDialog.h gui/widgets/display/StatisticsDialog.cpp
    gui/widgets/display/ProfileWindow.h gui/widgets/display/ProfileWindow.cpp
    gui/widgets/dialogs/RowRangeDialog.h gui/widgets/dialogs/RowRangeDialog.cpp
    gui/widgets/dialogs/ScaleControlDialog.h gui/widgets/dialogs/ScaleControlDialog.cpp
    gui/widgets/dialogs/DisplayStyleDialog.h gui/widgets/dialogs/DisplayStyleDialog.cpp
    gui/workers/FileSaverWorker.h gui/workers/FileSaverWorker.cpp
    gui/workers/FileLoaderWorker.h gui/workers/FileLoaderWorker.cpp
)
```

### Plugin `CMakeLists.txt` (mock, qhyccd, hamamatsu, picam)

Each plugin's POST_BUILD deploy currently targets `$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers`. All four must be updated to `$<TARGET_FILE_DIR:ezspeccam>/plugins/drivers` so the deployed binary side matches the new single target.

### Test CMakeLists

- `tests/test_metadata_save_load/CMakeLists.txt`: update paths `src/gui/workers/formats/*` → `src/app/formats/*`, `src/gui/workers/{FileLoaderWorker.cpp, IImageFormatHandler.h, SaveTypes.h}` → `src/app/formats/{FileLoaderWorker.cpp, IImageFormatHandler.h, SaveTypes.h}`.
- `tests/test_row_range_dialog/CMakeLists.txt`: same path updates.
- `tests/test_app_controller/CMakeLists.txt`: no path change needed (`AppController` still lives in `src/gui/`); the test only needs `AppController.h` to expose the new include (it does — `app/PluginLoader.h` is internal to `AppController.cpp`).
- `tests/test_cli/`: deleted (already disabled).

## End-to-End Verification (post-implementation)

```powershell
# Configure + build
.\build_preset.bat debug

# 1. CLI: list cameras (expect: mock-001 listed)
.\build\msvc-debug\bin\Debug\ezspeccam.exe --list

# 2. CLI: single TIFF capture (expect: tmp_cli\img_<ts>.tiff + _metadata.json with softwareSettings: {})
.\build\msvc-debug\bin\Debug\ezspeccam.exe --camera mock-001 --frames 1 --output .\tmp_cli

# 3. CLI: wide-format CSV (expect: row count == image height, no header line)
.\build\msvc-debug\bin\Debug\ezspeccam.exe --camera mock-001 --frames 1 --format csv --output .\tmp_csv
# Assert: (Get-Content .\tmp_csv\img_*.csv).Count == (mock image height)

# 4. CLI: sequence (configure + capture)
'{"steps":[{"configure":{"exposure":100}},{"capture":{"frames":1}}]}' | Out-File test_seq.json -Encoding ascii
.\build\msvc-debug\bin\Debug\ezspeccam.exe --sequence test_seq.json

# 5. GUI: launch (expect: MainWindow, PluginTab shows mock driver DLL file name)
.\build\msvc-debug\bin\Debug\ezspeccam.exe
# Manual check: window opens; "Available Cameras" table shows 1 row; close cleanly

# 6. Test suite (expect: all green)
.\run_tests.bat
```

## Risk Register (recap)

| Risk | Mitigation |
|---|---|
| CSV wide format breaks downstream scripts | Document as breaking change in `src/cli/README.md`; existing GUI tests already lock in wide format. |
| Metadata schema adds `softwareSettings` | GUI `FileLoaderWorker` already reads it; CLI was the odd one out. Verified by `test_metadata_save_load`. |
| AppController rewrite changes `PluginInfo` consumers | Only `PluginTab` consumes; it imports `app::plugins::Entry` directly. Field set is identical. |
| Plugin search path regression | `PluginLoader::scanDefaultRoots` uses the same two roots as current CLI. Verified by `test_plugin_loading`. |
| `tests/test_cli/` resurrection | Deleted. |
| Test CMakeLists path misses | Three tests updated; verify build via `.\run_tests.bat`. |
| Plugin POST_BUILD deploy target renamed | All 4 plugin `CMakeLists.txt` updated (mock, qhyccd, hamamatsu, picam); verified by `.\ezspeccam.exe --list` showing `mock-001`. |
| `build_preset.bat` / `run_tests.bat` affected | Neither script references the binary name; no change. |

## Implementation Order (top-level phases for `writing-plans`)

1. **Add new files in `src/app/`, keep old code compiling.** `AppController` still owns its own `PluginInfo`; CLI's `main.cpp` still standalone. `src/app/CMakeLists.txt` not yet created.
2. **Move format handlers** to `src/app/formats/`. `FileSaverWorker` and `FileLoaderWorker` start including from new location. `tests/test_metadata_save_load` updated.
3. **Refactor `AppController`** to use `app::plugins::scan()`. `tests/test_app_controller` updated.
4. **Move `SequenceRunner` and `WaitStabilizer` to `src/app/`.** CLI's `main.cpp` includes them from new location.
5. **Move `MessageHandler` to `src/app/`.** CLI includes it from new location.
6. **Extract `HeadlessController` and `CliOptions` / `CliMain`.** CLI's `main.cpp` becomes thin.
7. **Write `src/app/main.cpp`** with mode dispatch; add `src/gui/GuiMain.cpp`.
8. **Create `src/app/CMakeLists.txt`** as the single target; delete `src/cli/CMakeLists.txt`, `src/gui/CMakeLists.txt`, `src/cli/main.cpp`, `src/gui/main.cpp`.
9. **Update root `CMakeLists.txt`, `CMakePresets.json`, plugin POST_BUILDs.**
10. **Update test CMakeLists paths; delete `tests/test_cli/`.**
11. **Build + test + end-to-end smoke + doc updates.**

## Out of Scope

- No new CLI features (subcommands, new flags, configuration files beyond `--sequence`).
- No new GUI features.
- No changes to the `ICameraDriver` interface or `CameraTypes`.
- No changes to plugin authors' workflow.

## Open Questions (must resolve before plan finalisation)

1. **Where does `FileLoaderWorker` live?** — **Resolved: keep in `src/gui/workers/`.** Only the GUI opens files (the "Open File" dialog). The CLI never reads. `test_metadata_save_load` and `test_row_range_dialog` continue to include it from `src/gui/workers/`; no path change needed for this file. (The test for `test_metadata_save_load` does need its `formats/*` paths updated to `src/app/formats/*` — covered above.)
