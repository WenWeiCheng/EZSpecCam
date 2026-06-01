# src/core — Camera Driver Interface & Types

**Generated:** 2026-05-17

## OVERVIEW
Static library defining the camera driver contract (`ICameraDriver`) and core data types (ROIs, binning, parameters, errors, enums). Header-only interface; no implementation.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Driver contract | `ICameraDriver.h` / `ICameraDriver.md` | ~15 pure virtual methods + 5 signals; Q_DECLARE_INTERFACE |
| Core types | `CameraTypes.h` / `CameraTypes.md` | ROIs, binning, params, errors, enums — detailed type reference |
| Error codes | `CameraTypes.h` | `CameraError` struct (inlined in CameraTypes) |
| SDK include | `sdk/include/` | QHYCCD SDK headers |

## CONVENTIONS
- **C++17**, no extensions
- `Q_DECLARE_INTERFACE` macro for plugin identity
- Signals for async events: `frameReady()`, `connectionChanged()`, `errorOccurred()`, `captureStarted()`, `captureStopped()`
- Parameter definitions via `ParameterDefinition` struct (name, type, range, default)

## ANTI-PATTERNS
- **DO NOT** modify SDK headers in `sdk/include/` — vendor files
- **DO NOT** add implementation here — this is interface/types only
