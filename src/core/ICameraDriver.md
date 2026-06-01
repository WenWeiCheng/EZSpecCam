# ICameraDriver — Camera Driver Interface

**Generated:** 2026-06-01
**Commit:** c315e54
**Branch:** main

## OVERVIEW

`ICameraDriver` is the central abstraction for all camera hardware in EZSpecCam. Defined as a Qt interface (`Q_DECLARE_INTERFACE`), it establishes a contract of ~15 pure virtual methods and 5 signals that every camera driver plugin must fulfill. Drivers are loaded at runtime via `QPluginLoader` — no compile-time coupling to hardware SDKs.

**Design principles:**
- **Signal-driven frame delivery** — `frameReady()` replaces polling/buffer patterns; consumers connect once and receive frames asynchronously.
- **Staged parameters** — `setParameter()` / `validateParameters()` / `commitParameters()` three-phase pattern lets callers batch changes and validate before applying.
- **Minimal surface** — reduced from 17+ methods in the original design to ~10 pure virtuals, keeping driver implementations simple.
- **Plugin identity** — `Q_DECLARE_INTERFACE(ICameraDriver, "com.ezspeccam.ICameraDriver/1.0")` enables Qt's plugin system to discover and load drivers.

**Location:** `src/core/ICameraDriver.h`
**Plugin interface ID:** `com.ezspeccam.ICameraDriver/1.0`

---

## METHOD REFERENCE

### ——— Discovery ———

#### `enumerate()`
```cpp
virtual QStringList enumerate() = 0;
```
Scan the system for available cameras. Returns driver-specific identifiers (device paths, serial numbers, friendly names). These IDs are passed directly to `connectToCamera()`. Called before connection — no camera state required.

| Aspect | Detail |
|--------|--------|
| Returns | `QStringList` of camera IDs (empty if none found) |
| State | Valid in any state |
| Blocking | May block (hardware scan); callers should run off main thread for slow buses |

---

### ——— Connection ———

#### `connectToCamera(cameraId)`
```cpp
virtual bool connectToCamera(const QString &cameraId) = 0;
```
Establish a connection to the specified camera. On success, emits `connectionChanged(true, cameraId)`. On failure, emits `errorOccurred(CameraError)`.

| Aspect | Detail |
|--------|--------|
| `cameraId` | An identifier from `enumerate()` |
| Returns | `true` if connection succeeded |
| Signals | `connectionChanged(true, ...)` on success; `errorOccurred(...)` on failure |

#### `disconnectCamera()`
```cpp
virtual void disconnectCamera() = 0;
```
Gracefully terminate the active connection. Stops any in-progress capture, releases hardware resources, emits `connectionChanged(false, cameraId)`. No-op if already disconnected.

| Aspect | Detail |
|--------|--------|
| Returns | `void` |
| Side effects | Stops active capture; releases hardware |
| Signals | `connectionChanged(false, ...)` after teardown |

#### `isConnected()`
```cpp
virtual bool isConnected() const = 0;
```
Query connection state synchronously. Returns `true` only when a camera is fully connected and ready.

| Aspect | Detail |
|--------|--------|
| Returns | `bool` |
| Thread-safe? | Driver-dependent; prefer signal-driven state tracking |

---

### ——— Parameters ———

Parameters use a three-phase commit model:

```
setParameter()          ← stage individual changes
  ↓
validateParameters()    ← check all staged values
  ↓
commitParameters()      ← apply to hardware
```

#### `parameterNames()`
```cpp
virtual QStringList parameterNames() const = 0;
```
List all configurable and readable parameters this driver exposes. Names are driver-defined; used as keys for `parameter()`, `parameterValue()`, and `setParameter()`.

| Aspect | Detail |
|--------|--------|
| Returns | `QStringList` of parameter identifiers |
| When valid | After connection (parameters may be unavailable before) |

#### `parameter(name)`
```cpp
virtual ParameterDefinition parameter(const QString &name) const = 0;
```
Get the full metadata for a parameter: type (`FloatRange`, `IntCollection`, `Boolean`, etc.), range/choices, default value, category (`Core`, `Cooling`, `Info`, `Advanced`, `Debug`), read-only flag, and description. Returns default-constructed `ParameterDefinition` if name is unknown.

| Aspect | Detail |
|--------|--------|
| `name` | Parameter identifier from `parameterNames()` |
| Returns | `ParameterDefinition` struct |

#### `parameterValue(name)`
```cpp
virtual QVariant parameterValue(const QString &name) const = 0;
```
Read the current value of a parameter. Returns `QVariant::Invalid` if the parameter does not exist.

| Aspect | Detail |
|--------|--------|
| `name` | Parameter identifier |
| Returns | `QVariant` (type depends on parameter definition) |

#### `setParameter(name, value)`
```cpp
virtual bool setParameter(const QString &name, const QVariant &value) = 0;
```
Stage a new value for a parameter. The change is not applied to hardware until `commitParameters()` is called. Returns `false` if the value is rejected (out of range, wrong type). May emit `errorOccurred(...)` for invalid values.

| Aspect | Detail |
|--------|--------|
| `name` | Parameter identifier |
| `value` | New value (must match expected QVariant type) |
| Returns | `true` if the value was accepted for staging |

#### `validateParameters()`
```cpp
virtual bool validateParameters() = 0;
```
Check all staged parameter changes for validity without applying them to hardware. Useful for UI validation before commit. Returns `true` if all staged values are within valid ranges and consistent.

| Aspect | Detail |
|--------|--------|
| Returns | `true` if all parameters are valid |
| Side effects | None (read-only check) |

#### `commitParameters()`
```cpp
virtual bool commitParameters() = 0;
```
Apply all staged parameter changes to the camera hardware. After this call, `parameterValue()` should reflect the committed values. Returns `false` if any parameter failed to apply.

| Aspect | Detail |
|--------|--------|
| Returns | `true` if all changes were applied |
| Side effects | Modifies hardware state |

---

### ——— Capture ———

#### `startCapture(captureCount)`
```cpp
virtual bool startCapture(int captureCount = 0) = 0;
```
Begin frame acquisition. Frames are delivered asynchronously via `frameReady()`. After capture starts, emits `captureStarted(cameraId)`. If `captureCount > 0`, capture stops automatically after that many frames and emits `captureStopped(cameraId)`.

| Aspect | Detail |
|--------|--------|
| `captureCount` | Number of frames (0 = continuous until `stopCapture()`) |
| Returns | `true` if capture started |
| Signals | `captureStarted(...)` on success; `frameReady(...)` per frame |

#### `stopCapture(timeoutMs)`
```cpp
virtual void stopCapture(int timeoutMs = 5000) = 0;
```
Gracefully stop an active capture session. Waits up to `timeoutMs` for in-progress acquisitions to complete. Emits `captureStopped(cameraId)` after stopping.

| Aspect | Detail |
|--------|--------|
| `timeoutMs` | Maximum wait time in milliseconds (default 5000) |
| Signals | `captureStopped(...)` after stop completes |

---

### ——— Driver Info ———

#### `state()`
```cpp
virtual CameraState state() const = 0;
```
Get the current operational state of the camera/driver.

| Value | Meaning |
|-------|---------|
| `CameraState::Disconnected` | No camera connected |
| `CameraState::Connecting` | Connection in progress |
| `CameraState::Connected` | Connected, idle, ready for capture |
| `CameraState::Acquiring` | Actively capturing frames |
| `CameraState::Error` | Error state |

#### `driverVersion()`
```cpp
virtual QString driverVersion() const = 0;
```
Semantic version string for this driver implementation. Format: `"major.minor.patch"`. Used for logging, diagnostics, and compatibility checks.

#### `cameraId()`
```cpp
virtual QString cameraId() const = 0;
```
The identifier of the currently connected camera (the same string passed to `connectToCamera()`). Returns empty string if not connected.

---

## SIGNALS

All signals are asynchronous — emitted from the driver's thread/event loop. Consumers connect via Qt's signal-slot mechanism.

#### `frameReady(image, timestamp, frameNumber, cameraId, parameters)`
```cpp
void frameReady(const QSharedPointer<QImage> &image,
                quint64 timestamp,
                int frameNumber,
                const QString &cameraId,
                const QVariantMap &parameters);
```
Primary data delivery signal. Emitted for each captured frame. `QSharedPointer<QImage>` avoids unnecessary copies.
- `timestamp` — microseconds since epoch
- `frameNumber` — sequential (0-based per session)
- `parameters` — active parameter values at time of capture

#### `captureStarted(cameraId)`
Emitted when `startCapture()` succeeds and frames begin flowing.

#### `captureStopped(cameraId)`
Emitted when capture ends — either via `stopCapture()` or after `captureCount` frames.

#### `connectionChanged(connected, cameraId)`
Emitted on connect/disconnect. `connected` is `true` after successful `connectToCamera()`, `false` after `disconnectCamera()`.

#### `errorOccurred(error)`
```cpp
void errorOccurred(const CameraError &error);
```
Emitted on driver or camera errors. `CameraError` contains an error code and optional message string. Consumers should connect to this signal for error handling.

---

## KNOWN IMPLEMENTATIONS

| Driver | Location | Hardware | Notes |
|--------|----------|----------|-------|
| `QHYCCDDriver` | `src/plugins/qhyccd/` | QHYCCD scientific cameras | Real hardware; uses QHYCCD SDK |
| `HamamatsuDriver` | `src/plugins/hamamatsu/` | Hamamatsu cameras | Real hardware; uses Hamamatsu SDK |
| `MockCameraDriver` | `src/plugins/mock/` | Simulated camera | Generates synthetic frames for testing and development |

Each driver is a Qt plugin with a `plugin.json` descriptor (JSON metadata including driver name, version, and supported cameras).

---

## USAGE LIFECYCLE

A typical consumer (GUI `AppController`, CLI `CaptureController`) follows this flow:

```
1. DISCOVERY
   QPluginLoader scans plugin directories → loads ICameraDriver plugins
   driver->enumerate() → list of available camera IDs

2. CONNECTION
   driver->connectToCamera(cameraId) → connectionChanged(true) signal
   driver->parameterNames() / parameter() → discover available settings

3. CONFIGURATION (optional)
   driver->setParameter("exposure", 100.0)
   driver->setParameter("gain", 1.5)
   driver->validateParameters() → check before committing
   driver->commitParameters() → apply to hardware

4. CAPTURE
   driver->startCapture(count) → captureStarted() signal
   frameReady(...) signal ← received per frame
   driver->stopCapture() → captureStopped() signal

5. DISCONNECTION
   driver->disconnectCamera() → connectionChanged(false) signal
```

### Consumer Patterns

- **GUI (`AppController`)**: Maintains a state machine (`Disconnected → Connecting → Connected → Acquiring → Error`). Transitions driven by `connectionChanged` and `captureStarted`/`captureStopped` signals. Displays `frameReady` images in `ImageViewWidget`.
- **CLI (`CaptureController`)**: Sequential flow — connect, configure, capture N frames, disconnect. Uses `QEventLoop` + signal connections for synchronous-style control.

---

## ERROR HANDLING

Drivers report errors via the `errorOccurred(CameraError)` signal. Consumers should:

1. Always connect to `errorOccurred` before calling any driver method.
2. Check return values of synchronous methods (`connectToCamera()`, `startCapture()`, `commitParameters()`) — a `false` return indicates immediate failure.
3. Handle `errorOccurred` for asynchronous errors (capture failures, hardware disconnects).

```cpp
// Example: safe connect with error handling
connect(driver, &ICameraDriver::errorOccurred, this, [](const CameraError &e) {
    qWarning() << "Driver error:" << e.code << e.message;
});
connect(driver, &ICameraDriver::connectionChanged, this, [](bool connected, const QString &id) {
    if (connected) qInfo() << "Connected to" << id;
});
if (!driver->connectToCamera(cameraId)) {
    // Immediate failure — errorOccurred may also be emitted
}
```

---

## IMPLEMENTING A NEW DRIVER

To create a new camera driver plugin:

1. **Subclass `ICameraDriver`** — implement all 15 pure virtual methods.
2. **Add `Q_OBJECT`**, `Q_PLUGIN_METADATA`, and `Q_INTERFACES(ICameraDriver)` macros.
3. **Create `plugin.json`** — metadata descriptor (name, version, description).
4. **Add CMake target** in `src/plugins/<name>/CMakeLists.txt` — build as a Qt plugin (`MODULE` or `SHARED` library).
5. **Emit signals** at the correct lifecycle points:
   - `connectionChanged` on connect/disconnect
   - `frameReady` for each captured frame
   - `captureStarted` / `captureStopped` around capture sessions
   - `errorOccurred` on any hardware or protocol error

**Minimum viable implementation checklist:**
- [ ] `enumerate()` returns real camera IDs (or empty if hardware absent)
- [ ] `connectToCamera()` / `disconnectCamera()` manage hardware lifecycle
- [ ] `parameterNames()` returns at least the discovery-available parameters
- [ ] `startCapture()` / `stopCapture()` drive frame acquisition and emit `frameReady`
- [ ] `state()` accurately reflects current operational state
- [ ] `driverVersion()` returns the plugin's version string
- [ ] All signals are emitted at correct lifecycle points
- [ ] `plugin.json` exists with valid metadata

---

## SEE ALSO

- `src/core/CameraTypes.md` — data structures used throughout (ROIs, binning, `ParameterDefinition`, `CameraError`, enums)
- `src/core/CameraTypes.h` — header source
- `src/core/AGENTS.md` — overview of the core library
- `src/gui/AppController.h` — GUI driver consumer (state machine)
- `src/cli/CaptureController.h` — CLI driver consumer
- `src/plugins/AGENTS.md` — plugin development guide
- `src/plugins/mock/` — reference implementation for new driver authors
