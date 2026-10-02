# src/plugins — Camera Driver Plugins

**Generated:** 2026-05-17

## OVERVIEW
Qt plugin-based camera drivers implementing `ICameraDriver`. Loaded at runtime via `QPluginLoader`. Each plugin has a `.json` metadata descriptor.

## STRUCTURE
```
plugins/
├── CMakeLists.txt              # Shared plugin loader library
├── mock/                       # Simulated camera for development/testing
│   ├── CMakeLists.txt
│   ├── mock.json
│   ├── MockCameraDriver.cpp
│   └── MockCameraDriver.h
├── qhyccd/                     # Real QHYCCD hardware driver
│   ├── CMakeLists.txt
│   ├── qhyccd.json
│   ├── QHYCCDDriver.cpp
│   └── QHYCCDDriver.h
└── hamamatsu/                  # Real Hamamatsu DCAMSDK4 hardware driver
    ├── CMakeLists.txt
    ├── hamamatsu.json
    ├── HamamatsuDriver.cpp
    └── HamamatsuDriver.h
└── hk16011/                    # Real HK16011 CCD spectrometer driver (Linux only)
    ├── AGENTS.md               # Device quirks + SDK integration notes — read before touching capture
    ├── CMakeLists.txt
    ├── hk16011.json
    ├── Hk16011Driver.cpp
    └── Hk16011Driver.h
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Driver interface contract | `../core/ICameraDriver.h` |
| Add new driver | Copy mock/ structure → implement virtual methods → add `.json` descriptor |
| Plugin loading logic | `../gui/AppController.cpp` (scanPlugins + QPluginLoader) |
| Mock camera for testing | `mock/MockCameraDriver.cpp` — generates synthetic frames |
| QHYCCD hardware driver | `qhyccd/QHYCCDDriver.cpp` — QHYCCD SDK integration |
| Hamamatsu hardware driver | `hamamatsu/HamamatsuDriver.cpp` — DCAMSDK4 integration |
| HK16011 hardware driver | `hk16011/Hk16011Driver.cpp` — libHK16011 integration; read `hk16011/AGENTS.md` first, the device has several frame-delivery traps |

## CONVENTIONS
- Each driver is a Qt plugin: inherits `QObject` + `ICameraDriver`, uses `Q_PLUGIN_METADATA`
- `.json` descriptor file required for `Q_PLUGIN_METADATA` — `"Keys": ["<plugin-name>"]` format
- Signal-based: `frameReady()`, `connectionChanged()`, `errorOccurred()`, `captureStarted()`, `captureStopped()`
- Each driver should implement `ICameraDriver`, but each plugin driver can have its own parameters defined in `initializeParameterDefinitions()`
- Define a slot named `onCaptureCompleted` to trigger `stopCapture()`. Use `QMetaObject::invokeMethod()` to call it when `onCaptureLoop()` completes. `stopCapture()` cannot use `QMetaObject::invokeMethod()` because it's a slot
- SDK files in `sdk/` subdirectory are vendor files — **do not modify**
- Drivers with vendor SDKs (qhyccd, hamamatsu) use static reference counting for SDK lifecycle management

## ANTI-PATTERNS
- **DO NOT** have the same plugin `.json` descriptor name as another plugin — `QPluginLoader` resolves by name
- **DO NOT** block the main thread in camera drivers — use async signals
- **DO NOT** modify SDK files in `sdk/` subdirectories — vendor-supplied files
- **DO NOT** hardcode parameter constraints — query from SDK at runtime (e.g., hamamatsu uses `dcamprop_getnextid()`)
