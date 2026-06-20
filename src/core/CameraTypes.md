# CameraTypes — Core Data Structures

**Generated:** 2026-06-01
**Commit:** c315e54
**Branch:** main

## OVERVIEW

`CameraTypes.h` defines all data structures, enums, and validation functions consumed by `ICameraDriver` and its implementations. This is a header-only file — all types are value types with comparison operators and `isValid()` guards, designed for stack allocation and Qt signal/slot transport via `Q_DECLARE_METATYPE`.

**Dependencies:** Qt 6 (`QString`, `QImage`, `QVariant`, `QVector`)

---

## ENUMS

### `CameraState` — Operational State Machine

```cpp
enum class CameraState { Disconnected, Connecting, Connected, Acquiring, Error };
```

Tracks the lifecycle of a camera driver. Consumers (GUI `AppController`, CLI `main.cpp`) use this for UI state and workflow gating.

```
Disconnected ──→ Connecting ──→ Connected ──→ Acquiring
     ↑               │               │              │
     └───────────────┴───────────────┴──────────────┘
                    Error (any state → Error)
```

| Value | Meaning | Entered by |
|-------|---------|------------|
| `Disconnected` | No camera connected; idle | Constructor, `disconnectCamera()` |
| `Connecting` | Connection in progress | Start of `connectToCamera()` |
| `Connected` | Ready for parameter config or capture | Successful `connectToCamera()` |
| `Acquiring` | Frames flowing | `startCapture()` success |
| `Error` | Recoverable or fatal error | `errorOccurred` signal, any failed operation |

---

### `ParameterType` — Value Domain Classification

```cpp
enum class ParameterType { FloatRange, FloatCollection, IntRange, IntCollection, String, StringCollection, Boolean };
```

Determines how a parameter's value is validated and what UI widget is appropriate.

| Type | Validation | Typical UI Widget | Example |
|------|-----------|-------------------|---------|
| `FloatRange` | `min ≤ val ≤ max` | QDoubleSpinBox | Exposure time (ms) |
| `FloatCollection` | `val ∈ {values}` | QComboBox | Predefined gain values |
| `IntRange` | `min ≤ val ≤ max` | QSpinBox | Binning factor |
| `IntCollection` | `val ∈ {values}` | QComboBox | Bit depth |
| `String` | Can convert to `QString` | QLabel | Camera serial number |
| `StringCollection` | `val ∈ {values}` | QComboBox | Readout mode |
| `Boolean` | Can convert to `bool` | QCheckBox | Cooling enable |

---

### `ParameterCategory` — UI Grouping

```cpp
enum class ParameterCategory { Core, Cooling, Info, Advanced, Debug };
```

Drives tab/layout placement in the GUI parameter panel. Drivers assign each parameter a category; the UI groups them accordingly.

| Category | Intended content | Typical visibility |
|----------|-----------------|-------------------|
| `Core` | Exposure, gain, offset | Always visible; first tab |
| `Cooling` | Target temperature, cooler status | Visible when cooler hardware present |
| `Info` | Read-only: serial, firmware version | Info panel / about dialog |
| `Advanced` | Binning, ROI, trigger mode | Collapsible or secondary tab |
| `Debug` | Raw register values, debug flags | Hidden by default |

---

## PARAMETER SYSTEM

The parameter system is the core abstraction for camera configuration — it lets drivers expose any camera setting in a uniform way that the GUI can render without camera-specific code.

### `ParameterConstraint` — Value Constraints

```cpp
struct ParameterConstraint {
    double minValue, maxValue, step;   // for Range types
    QVector<QVariant> validValues;     // for Collection types
    QStringList unit;                  // unit labels (e.g. ["ms", "s", "min"])
    QVector<double> unitRange;         // unit boundary thresholds
    /* + hasUnitRange(), getUnitIndex(), toDisplayValue(), toRawValue(), isValid(), ==, != */
};
```

**Field matrix by ParameterType:**

| Field | FloatRange | FloatCollection | IntRange | IntCollection | String | StringCollection | Boolean |
|-------|-----------|-----------------|----------|---------------|--------|-------------------|---------|
| `minValue` | ✓ | — | ✓ | — | — | — | — |
| `maxValue` | ✓ | — | ✓ | — | — | — | — |
| `step` | ✓ | — | ✓ | — | — | — | — |
| `validValues` | — | ✓ | — | ✓ | — | ✓ | — |
| `unit` | optional | - | optional | - | — | — | — |

**Unit system:** When `unit` and `unitRange` are populated, the constraint supports automatic unit conversion. Example: exposure with `unit = ["μs", "ms", "s"]` and `unitRange = [1000, 1000]` means:
- 0–999 → display as μs
- 1000–999999 → display as ms (divide by 1000)
- ≥1000000 → display as s (divide by 1000000)

Drivers store raw values; `toDisplayValue()` and `toRawValue()` handle conversion for UI.

### `ParameterDefinition` — Full Parameter Metadata

```cpp
struct ParameterDefinition {
    QString name;              // unique identifier (key for all parameter APIs)
    QString displayName;       // human-readable label for UI
    QString description;       // tooltip / help text
    ParameterCategory category;
    ParameterType type;
    ParameterConstraint constraint;
    QVariant defaultValue;
    bool isReadOnly = false;
    bool isDynamic = false;    // value can change at runtime (e.g. temperature), or can change when other parameters change (e.g. roi)
    bool isExtrinsic = false;  // changes due to environment, not user-controllable
    bool needReconnect = false;// changing this requires disconnect/reconnect
    float order = 10000000.0f; // UI sort order (lower = higher priority)
    /* + isValid(), ==, != */
};
```

**Key flags:**

| Flag | Effect |
|------|--------|
| `isReadOnly` | UI renders as label, not editable widget |
| `isDynamic` | UI polls `parameterValue()` periodically for updates |
| `isExtrinsic` | Shown in UI but greyed out; changes via `errorOccurred` or polling |
| `needReconnect` | Changing value triggers disconnect/reconnect prompt in GUI |

**`isValid()` logic:**
- Name must be non-empty.
- Non-read-only parameters must have valid constraints.
- Non-`Info` category parameters must have a default value that passes validation.

### `validate()` / `validateReason()` — Free Functions

```cpp
bool validate(const QVariant &value, const ParameterConstraint &constraint, ParameterType type);
QString validateReason(const QVariant &value, const ParameterConstraint &constraint, ParameterType type);
```

Used by drivers (`validateParameters()`, `commitParameters()`) and GUI (pre-submit checks). `validate()` returns `true/false`; `validateReason()` returns a human-readable explanation on failure (e.g. `"Value 500 is above maximum 200"`).

Both are `inline` — defined in the header for zero-cost inclusion.

---

## ERROR HANDLING

### `CameraError` — Error Report

```cpp
struct CameraError {
    enum class Code { None, InvalidParameter, ValueOutOfRange, CommitFailed, HardwareFault,
                      NotConnected, NotSupported, ConnectionFailed, CaptureFailed, Timeout,
                      StateInvalid, DriverError, PluginLoadFailed, CommunicationError };
    enum class Severity { Info, Warning, Error, Fatal };
    Code code = Code::None;
    Severity severity = Severity::Info;
    QString parameterName;   // affected parameter (if applicable)
    QString description;      // human-readable message
    bool recoverable = true;  // false = fatal, disconnect required
    /* + hasError(), success(), makeError(), ==, != */
};
```

**Severity semantics:**

| Severity | UI Treatment | Example |
|----------|-------------|---------|
| `Info` | Status bar / log | "Cooler reached target temperature" |
| `Warning` | Yellow banner, non-blocking | "ROI partially outside sensor, clamped" |
| `Error` | Red banner, operation aborted | "Failed to set exposure" |
| `Fatal` | Dialog + disconnect | "Camera hardware fault" |

**Factory helpers:**
- `CameraError::success()` — no error (code = `None`).
- `CameraError::makeError(code, desc, severity)` — one-liner for common cases.

**Error codes and typical causes:**

| Code | Typical trigger |
|------|----------------|
| `InvalidParameter` | Unknown parameter name |
| `ValueOutOfRange` | Value outside `ParameterConstraint` bounds |
| `CommitFailed` | Hardware rejected staged changes |
| `HardwareFault` | Camera reported internal error |
| `NotConnected` | Operation attempted while disconnected |
| `NotSupported` | Feature not supported by this camera model |
| `ConnectionFailed` | `connectToCamera()` failure |
| `CaptureFailed` | `startCapture()` failure or mid-capture error |
| `Timeout` | Operation exceeded time limit |
| `StateInvalid` | Operation invalid for current `CameraState` |
| `DriverError` | SDK-level error |
| `PluginLoadFailed` | `QPluginLoader` could not load driver |
| `CommunicationError` | USB/Ethernet/Serial transport error |

Emitters: `ICameraDriver::errorOccurred(CameraError)` signal.

---

## IMAGE & FRAME DATA

### `ImageData` — Captured Frame Container

```cpp
struct ImageData {
    QImage image;                  // processed/display image
    QImage originalImage;          // raw image before any processing
    QVector<quint64> spectrum;     // 1D spectrum (populated when VerticalBinning enabled)
    quint64 timestamp;             // ns since epoch
    int frameNumber;               // sequential within session, reset on disconnect
    QString cameraId;
    QVariantMap parameters;        // parameter snapshot at capture time
    /* + isValid(), hasOriginal() */
};
```

`isValid()` requires `!image.isNull() && timestamp > 0`. On the driver side, frames are delivered via `ICameraDriver::frameReady(QSharedPointer<QImage>, ...)` — `ImageData` is the consumer-side wrapper used in CLI workflows (`src/cli/main.cpp::captureFrames`) and test assertions.

---

## Q_DECLARE_METATYPE REGISTRATIONS

The following types are registered for Qt's meta-object system, enabling their use in signals/slots and `QVariant`:

```cpp
Q_DECLARE_METATYPE(ParameterType)
Q_DECLARE_METATYPE(ParameterCategory)
Q_DECLARE_METATYPE(ParameterConstraint)
Q_DECLARE_METATYPE(ParameterDefinition)
Q_DECLARE_METATYPE(CameraError)
Q_DECLARE_METATYPE(CameraError::Code)
Q_DECLARE_METATYPE(CameraError::Severity)
Q_DECLARE_METATYPE(CameraState)
```

Without these, passing these types across queued signal/slot connections would fail at runtime.

---

## TYPE DEPENDENCY MAP

```
CameraState (enum)
ParameterType (enum)
ParameterCategory (enum)
    │
    ▼
ParameterConstraint ──────► ParameterDefinition
                                │
                                ▼
CameraError ◄── errorOccurred ── ICameraDriver ── frameReady ──► ImageData
```

---

## SEE ALSO

- `src/core/ICameraDriver.md` — the interface that consumes all types defined here
- `src/core/ICameraDriver.h` — interface definition with signals
- `src/plugins/mock/` — reference driver showing parameter definition patterns
- `src/gui/widgets/config/CameraTab.h` — GUI that renders parameters from `ParameterDefinition`
