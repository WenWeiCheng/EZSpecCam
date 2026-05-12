# src/plugins — Camera Driver Plugins

**Generated:** 2026-05-12

## OVERVIEW
Qt plugin-based camera drivers implementing `ICameraDriver`. Loaded at runtime via `QPluginLoader`. Each plugin has a `.json` metadata descriptor.

## STRUCTURE
```
plugins/
├── CMakeLists.txt              # Shared plugin loader library
├── mock/                       # Simulated camera for development/testing
│   ├── CMakeLists.txt
│   ├── mock.json               # Plugin metadata
│   ├── MockCameraDriver.cpp    # ~700 lines
│   └── MockCameraDriver.h
└── qhyccd/                     # Real QHYCCD hardware driver (DISABLED)
    ├── CMakeLists.txt
    ├── qhyccd.json
    ├── QHYCCDDriver.cpp        # ~580 lines
    └── QHYCCDDriver.h
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Driver interface contract | `../core/ICameraDriver.h` |
| Add new driver | Copy mock/ structure → implement virtual methods → add `.json` descriptor |
| Plugin loading logic | `../gui/AppController.cpp` (scanPlugins + QPluginLoader) |
| Mock camera for testing | `mock/MockCameraDriver.cpp` — generates synthetic frames |

## CONVENTIONS
- Each driver is a Qt plugin: inherits `QObject` + `ICameraDriver`, uses `Q_PLUGIN_METADATA`
- `.json` descriptor file required for `Q_PLUGIN_METADATA`
- Signal-based: `frameReady()`, `connectionChanged()`, `errorOccurred()`, `captureStarted()`, `captureStopped()`
- `enumerate()` returns camera IDs for discovery
- State management: track `CameraState` enum internally

## ANTI-PATTERNS
- **DO NOT** have the same plugin `.json` descriptor name as another plugin — `QPluginLoader` resolves by name
- QHYCCD plugin is **disabled in CMake** (comment out `add_subdirectory` to re-enable)
