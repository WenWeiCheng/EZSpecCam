# src/plugins/picam — Teledyne Princeton Instruments PICam 5.x Hardware Driver

**Generated:** 2026-06-01

Real Princeton Instruments camera hardware driver implementing `ICameraDriver`. Communicates with physical camera via PICam 5.x SDK (`picam.dll`). Supports all PICam-compatible cameras (PIXIS, ProEM, PyLoN, etc.).

---

## PICam SDK DESIGN OVERVIEW

PICam SDK is a **C API** with ~40 functions that uses an **enum-based parameter namespace**. Every camera feature is represented by a `PicamParameter` enum value. The SDK provides typed getter/setter pairs per value type:

| Value Type | Getter | Setter |
|-----------|--------|--------|
| `Integer` | `Picam_GetParameterIntegerValue()` | `Picam_SetParameterIntegerValue()` |
| `Boolean` | `Picam_GetParameterBooleanValue()` | `Picam_SetParameterBooleanValue()` |
| `FloatingPoint` | `Picam_GetParameterFloatingPointValue()` | `Picam_SetParameterFloatingPointValue()` |
| `LargeInteger` | `Picam_GetParameterLargeIntegerValue()` | `Picam_SetParameterLargeIntegerValue()` |
| `Enumeration` | typed via integer getter + `Picam_GetEnumerationString()` | typed via integer setter |
| `Rois` | `Picam_GetParameterRoisValue()` | `Picam_SetParameterRoisValue()` |
| `Pulse` | `Picam_GetParameterPulseValue()` | `Picam_SetParameterPulseValue()` |
| `Modulations` | `Picam_GetParameterModulationsValue()` | `Picam_SetParameterModulationsValue()` |

All parameter changes are **staged locally**, then applied atomically via `Picam_CommitParameters()` — which mirrors EZSpecCam's `commitParameters()` pattern.

---

## WHERE TO LOOK

| Task | Location |
|------|----------|
| PICam Programmer's Manual | `driver-sdk/picam/doc/PICam 5.x Programmer's Manual.pdf` |
| PICam samples | `driver-sdk/picam/Samples/source/platform_independent/` |
| `configure.cpp` sample | `driver-sdk/picam/Samples/source/platform_independent/configure.cpp` — exposure, gain, commit |
| `param_info.cpp` sample | `driver-sdk/picam/Samples/source/platform_independent/param_info.cpp` — full parameter introspection (857 lines) |
| `rois.cpp` sample | `driver-sdk/picam/Samples/source/platform_independent/rois.cpp` — single & multi ROI setup |
| `acquire.cpp` sample | `driver-sdk/picam/Samples/source/platform_independent/acquire.cpp` — simplest acquisition (69 lines) |
| Driver interface contract | `src/core/ICameraDriver.h` |
| Parameter type definitions | `src/core/CameraTypes.h` |
| Mock implementation (reference) | `src/plugins/mock/MockCameraDriver.cpp` |
| Hamamatsu implementation (reference) | `src/plugins/hamamatsu/HamamatsuDriver.cpp` |

## Samples

In `driver-sdk/picam/Samples/source/platform_independent/`:

| Sample | Explanation |
| :--- | :--- |
| `acquire` | Simplest acquisition loop — initialize → open → acquire → close |
| `configure` | Set exposure/gain, commit, acquire, display mean intensity |
| `rois` | Single ROI setup (centered half-size) and multiple ROI patterns |
| `param_info` | **Full parameter introspection** — iterates all parameters, prints name/ID/value/type/access/constraints/relevance/dynamics/extrinsic/volatile/waitable |
| `gating` | Repetitive & sequential gating modes (intensified cameras) |
| `kinetics` | Kinetics acquisition mode |
| `metadata` | Frame timestamps and metadata tracking |
| `multicam` | Open and control multiple cameras simultaneously |
| `poll` | Non-blocking acquisition via polling |
| `save_data` | Write acquired frames to file |
| `spectrograph` | Acton/Princeton Instruments spectrograph control |
| `wait_for_trig` | Hardware trigger synchronization |

---

## PARAMETER MAPPING SYSTEM

### Value Type Mapping

Picam uses 8 value types. EZSpecCam uses 7 parameter types. The driver MUST implement an adapter layer that routes typed Picam getter/setter calls based on `Picam_GetParameterValueType()`.

| Picam `PicamValueType` | EZSpecCam `ParameterType` | Mapping Rule |
|------------------------|---------------------------|--------------|
| `Integer` | `IntRange` | Direct — `piint` ↔ `QVariant(int)` |
| `Boolean` | `Boolean` | Direct — `pibln` ↔ `QVariant(bool)` |
| `FloatingPoint` | `FloatRange` | Direct — `piflt` ↔ `QVariant(double)` |
| `LargeInteger` | `IntRange` | Convert — `pi64s` ↔ `QVariant(qlonglong)`; constrain range if > 32-bit |
| `Enumeration` | `StringCollection` | **Preferred** — resolve integer enum value to string via `Picam_GetEnumerationString()`, store strings in `constraint.validValues`. Fallback: `IntCollection` if string resolution fails |
| `Rois` | **Compound** | Decomposed into 4+ separate scalar parameters — see ROI Handling below |
| `Pulse` | **Skipped or Flat** | No EZSpecCam equivalent. Options: (a) skip for non-intensified cameras, (b) decompose into `pulse_delay`/`pulse_width` scalar params |
| `Modulations` | **Skipped** | No EZSpecCam equivalent. Skip for now; only needed by specific camera models |

### Constraint Mapping

Picam has 5 constraint types queried via typed `Picam_GetParameter*Constraint()` functions. All map to EZSpecCam's single `ParameterConstraint` struct:

| Picam Constraint Type | Query Function | → `ParameterConstraint` Fields |
|-----------------------|---------------|-------------------------------|
| `Range` | `Picam_GetParameterRangeConstraint()` | `minValue` ← `minimum`, `maxValue` ← `maximum`, `step` ← `increment` |
| `Collection` | `Picam_GetParameterCollectionConstraint()` | `validValues` ← `values_array` (converted to `QVector<QVariant>`) |
| `Rois` | `Picam_GetParameterRoisConstraint()` | Decomposed — apply to individual scalar ROI params (see below) |
| `Pulse` | `Picam_GetParameterPulseConstraint()` | Skip or decompose |
| `Modulations` | `Picam_GetParameterModulationsConstraint()` | Skip |
| `None` | (no constraint) | Leave `constraint` at defaults; mark as `isReadOnly` or Info category |

**Constraint Category**: Picam distinguishes `Capable` (hardware capability) from `Required` (current context). **Always query Capable** for constraint display.

### Metadata Mapping

| Picam Property | Query API | EZSpecCam `ParameterDefinition` Field |
|---------------|-----------|--------------------------------------|
| `ValueAccess_ReadOnly` | `Picam_GetParameterValueAccess()` | `isReadOnly = true` |
| Can change during acquisition | `Picam_CanSetParameterOnline()` | Store as custom field or log only |
| Volatile (must read from HW) | `Picam_CanReadParameter()` | `isDynamic = true` |
| Extrinsic (externally changed) | `PicamAdvanced_GetParameterExtrinsicDynamics()` | `isExtrinsic = true` |
| Dynamic (can change internally) | `PicamAdvanced_GetParameterDynamics()` | `isDynamic = true` if any dynamics masked |
| Relevant in current mode | `Picam_IsParameterRelevant()` | Runtime filter — exclude irrelevant params from `parameterNames()` |
| Waitable status | `Picam_CanWaitForStatusParameter()` | No direct mapping; informational only |

---

## ROI HANDLING — DECOMPOSITION STRATEGY

### The Problem

Picam represents ROI as a **single composite parameter** `PicamParameter_Rois` with type `PicamRois`:

```c
typedef struct {
    piint x;
    piint y;
    piint width;
    piint height;
    piint x_binning;
    piint y_binning;
} PicamRoi;

typedef struct {
    piint roi_count;
    const PicamRoi* roi_array;  // dynamic array, allocated by SDK
} PicamRois;
```

EZSpecCam represents ROI as **separate scalar parameters**: `roi_x`, `roi_y`, `roi_width`, `roi_height`, plus `binning`.

### Solution: Decompose in Driver

**Phase 1 — Single ROI only** (most common case):

The driver decomposes `PicamParameter_Rois` into these EZSpecCam parameters:

| EZSpecCam Name | Type | Source | Constraint Source |
|---------------|------|--------|-------------------|
| `roi_x` | `IntRange` | `roi_array[0].x` | `RoisConstraint.x_constraint` |
| `roi_y` | `IntRange` | `roi_array[0].y` | `RoisConstraint.y_constraint` |
| `roi_width` | `IntRange` | `roi_array[0].width` | `RoisConstraint.width_constraint` |
| `roi_height` | `IntRange` | `roi_array[0].height` | `RoisConstraint.height_constraint` |
| `roi_x_binning` | `IntCollection` | `roi_array[0].x_binning` | `RoisConstraint.x_binning_limits` |
| `roi_y_binning` | `IntCollection` | `roi_array[0].y_binning` | `RoisConstraint.y_binning_limits` |

**Implementation Pattern:**

```
READ  (parameterValue):
  1. Check if the requested param is an ROI sub-param
  2. If ROI not yet cached, call Picam_GetParameterRoisValue(PicamParameter_Rois)
  3. Return the corresponding field from roi_array[0]
  4. Call Picam_DestroyRois() to free SDK-allocated memory

WRITE (setParameter):
  1. Cache the incoming sub-param value locally
  2. On commitParameters(), assemble a PicamRois from cached sub-params
  3. Call Picam_SetParameterRoisValue(PicamParameter_Rois, &assembled)
  4. Call Picam_CommitParameters()
```

**Constraints** are decomposed similarly — query `Picam_GetParameterRoisConstraint(PicamParameter_Rois, Capable)` once, then extract individual axis constraints for each sub-parameter's `ParameterConstraint`.

### Phase 2 — Multi-ROI (future consideration)

Some cameras support `roi_count > 1`. For now, handle only `roi_array[0]`. Multi-ROI could be exposed as EZSpecCam parameters with indexed names (`roi_0_x`, `roi_1_x`, etc.) or as an opaque complex parameter if EZSpecCam later adds complex type support.

---

## PARAMETER CATEGORIES

Parameters are discovered at runtime via `Picam_GetParameters()` — NOT hardcoded. Categorization is inferred from Picam parameter names and types.

### Category Assignment Rules

| Condition | `ParameterCategory` |
|-----------|---------------------|
| Read-only sensor/version/firmware info | `Info` |
| Temperature-related parameters | `Cooling` |
| Exposure, gain, ROI, binning, trigger | `Core` |
| ADC settings, readout rate, timing calibrations | `Advanced` |
| Debug/test parameters | `Debug` |

### Typical Parameter Set (from `param_info.cpp` introspection)

These are the most common parameters across PICam camera models. **Not all cameras support all parameters** — filter by `IsRelevant` at runtime.

#### Category: Core

| Picam Parameter (enum name) | EZSpecCam Name | Type | Notes |
|---------------------------|---------------|------|-------|
| `PicamParameter_ExposureTime` | `exposure` | `FloatRange` | milliseconds |
| `PicamParameter_AdcAnalogGain` | `analog_gain` | `StringCollection` | Enum: Low/Medium/High |
| `PicamParameter_AdcSpeed` | `adc_speed` | `StringCollection` | Enum: readout rates |
| `PicamParameter_AdcQuality` | `adc_quality` | `StringCollection` | Enum: speed vs noise |
| `PicamParameter_PixelFormat` | `pixel_format` | `StringCollection` | Monochrome16Bit, etc. |
| `PicamParameter_PixelBitDepth` | `bit_depth` | `IntCollection` | 8, 12, 14, 16 |
| `PicamParameter_ReadoutStride` | `readout_stride` | `IntRange` | Read-only, calculated |
| `PicamParameter_FrameSize` | `frame_size` | `IntRange` | Read-only, calculated |

#### Category: Core — ROI (decomposed)

| Picam Parameter | EZSpecCam Name | Type | Notes |
|----------------|---------------|------|-------|
| `PicamParameter_Rois` | `roi_x` | `IntRange` | Decomposed from `roi_array[0].x` |
| `PicamParameter_Rois` | `roi_y` | `IntRange` | Decomposed from `roi_array[0].y` |
| `PicamParameter_Rois` | `roi_width` | `IntRange` | Decomposed from `roi_array[0].width` |
| `PicamParameter_Rois` | `roi_height` | `IntRange` | Decomposed from `roi_array[0].height` |
| `PicamParameter_Rois` | `roi_x_binning` | `IntCollection` | Decomposed from `roi_array[0].x_binning` |
| `PicamParameter_Rois` | `roi_y_binning` | `IntCollection` | Decomposed from `roi_array[0].y_binning` |

#### Category: Cooling

| Picam Parameter | EZSpecCam Name | Type | Notes |
|----------------|---------------|------|-------|
| `PicamParameter_SensorTemperatureReading` | `sensor_temperature` | `FloatRange` | Read-only, °C |
| `PicamParameter_SensorTemperatureSetPoint` | `temperature_setpoint` | `FloatRange` | RW, °C |

#### Category: Info (Read-only)

| Picam Parameter | EZSpecCam Name | Type | Notes |
|----------------|---------------|------|-------|
| `PicamParameter_SensorWidth` | `sensor_width` | `IntRange` | Read-only |
| `PicamParameter_SensorHeight` | `sensor_height` | `IntRange` | Read-only |
| `PicamParameter_CameraModel` | `camera_model` | `String` | Read-only |
| `PicamParameter_SerialNumber` | `serial_number` | `String` | Read-only |
| `PicamParameter_FirmwareVersion` | `firmware_version` | `String` | Read-only |
| `PicamParameter_FirmwareRevision` | `firmware_revision` | `String` | Read-only |
| `PicamParameter_ChipName` | `chip_name` | `String` | Read-only |

---

## IMPLEMENTATION GUIDE

### File Structure

```
src/plugins/picam/
├── CMakeLists.txt
├── picam.json
├── PicamDriver.h
└── PicamDriver.cpp
```

### CMakeLists.txt Template

```cmake
# Princeton Instruments PICam camera driver plugin
qt_add_plugin(picam_camera_driver MODULE)

target_link_libraries(picam_camera_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Widgets
    picam  # PICam SDK import library
)

target_include_directories(picam_camera_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${PICAM_SDK_INCLUDE_DIR}  # path to picam.h
)

target_sources(picam_camera_driver PRIVATE
    PicamDriver.h PicamDriver.cpp
    picam.json
)

# Deploy to plugins/drivers/
add_custom_command(TARGET picam_camera_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:picam_camera_driver>"
        "$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers/"
    COMMENT "Deploying picam_camera_driver to plugins/drivers/"
)
```

### Plugin Metadata (`picam.json`)

```json
{
    "Keys": ["picam"]
}
```

### Class Skeleton

```cpp
// PicamDriver.h
class PicamDriver : public ICameraDriver {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.ezspeccam.ICameraDriver" FILE "picam.json")
    Q_INTERFACES(ICameraDriver)

public:
    explicit PicamDriver(QObject *parent = nullptr);
    ~PicamDriver() override;

    // ICameraDriver interface
    QStringList enumerate() override;
    bool connectToCamera(const QString &cameraId) override;
    void disconnectCamera() override;
    bool isConnected() const override;

    QStringList parameterNames() const override;
    ParameterDefinition parameter(const QString &name) const override;
    QVariant parameterValue(const QString &name) const override;
    bool setParameter(const QString &name, const QVariant &value) override;
    bool validateParameters() override;
    bool commitParameters() override;

    bool startCapture(int captureCount) override;
    void stopCapture() override;
    bool isCapturing() const override;

    CameraError lastError() const override;
    CameraState state() const override;

private:
    void initializeParameterDefinitions();
    void syncAllValuesFromHardware();

    PicamHandle m_handle = nullptr;
    QMap<QString, ParameterDefinition> m_paramDefs;
    QMap<QString, QVariant> m_params;           // committed values
    QMap<QString, QVariant> m_pendingParams;     // staged values
    QMap<QString, PicamParameter> m_paramEnumMap; // name → PicamParameter enum
    // ROI cache — refreshed on commit
    PicamRois m_cachedRois;
};
```

### `initializeParameterDefinitions()` Algorithm

```
1. Call Picam_GetParameters(m_handle, &params, &count)
2. For each PicamParameter in params:
   a. Get display name: Picam_GetEnumerationString(PicamEnumeratedType_Parameter, param)
   b. Get value type: Picam_GetParameterValueType()
   c. Get access: Picam_GetParameterValueAccess() → isReadOnly
   d. Get relevance: Picam_IsParameterRelevant() — skip if not relevant
   e. Get dynamics: PicamAdvanced_GetParameterDynamics() → isDynamic
   f. Get extrinsic: PicamAdvanced_GetParameterExtrinsicDynamics() → isExtrinsic
   g. Get constraint type and constraint values
   h. Map value type → ParameterType (see mapping table above)
   i. Build ParameterDefinition with name, type, constraint, flags
   j. Determine category from parameter name/semantics
   k. Insert into m_paramDefs and m_paramEnumMap
3. Handle PicamParameter_Rois specially — replace with decomposed sub-params
   (roi_x, roi_y, roi_width, roi_height, roi_x_binning, roi_y_binning)
4. Set parameter order for UI (Core=1-100, Cooling=101-200, Info=201-300, Advanced=301-400)
```

### `parameterValue()` / `setParameter()` Dispatch

The value type determines which Picam API to call. Store the `PicamValueType` alongside each parameter definition:

```cpp
QVariant PicamDriver::parameterValue(const QString &name) const
{
    // For ROI sub-params, extract from m_cachedRois
    if (isRoiSubParam(name))
        return getRoiSubValue(name);
    
    PicamParameter picamParam = m_paramEnumMap[name];
    PicamValueType vt = getValueType(name);
    
    switch (vt) {
    case PicamValueType_Integer: {
        piint v; Picam_GetParameterIntegerValue(m_handle, picamParam, &v);
        return QVariant(static_cast<int>(v));
    }
    case PicamValueType_Boolean: {
        pibln v; Picam_GetParameterBooleanValue(m_handle, picamParam, &v);
        return QVariant(v != 0);
    }
    case PicamValueType_FloatingPoint: {
        piflt v; Picam_GetParameterFloatingPointValue(m_handle, picamParam, &v);
        return QVariant(static_cast<double>(v));
    }
    case PicamValueType_LargeInteger: {
        pi64s v; Picam_GetParameterLargeIntegerValue(m_handle, picamParam, &v);
        return QVariant(static_cast<qlonglong>(v));
    }
    case PicamValueType_Enumeration: {
        piint v; Picam_GetParameterIntegerValue(m_handle, picamParam, &v);
        // Return the string representation for UI
        PicamEnumeratedType etype;
        Picam_GetParameterEnumeratedType(m_handle, picamParam, &etype);
        const pichar* str;
        Picam_GetEnumerationString(etype, v, &str);
        QString result(str);
        Picam_DestroyString(str);
        return QVariant(result);
    }
    default:
        return QVariant();
    }
}
```

### ROI Sub-Parameter Handling

ROI sub-parameters (`roi_x`, `roi_y`, `roi_width`, `roi_height`, `roi_x_binning`, `roi_y_binning`) require special handling because they come from a single Picam parameter:

**Reading**: Cache `PicamRois` on `commitParameters()`. When `parameterValue()` is called for an ROI sub-param, extract from cache.

**Writing**: Cache the sub-param value in `m_pendingParams`. On `commitParameters()`, assemble a complete `PicamRois` struct from all pending ROI sub-params, call `Picam_SetParameterRoisValue()`, then commit.

**Constraints**: Query `Picam_GetParameterRoisConstraint()` once and decompose into individual constraints for each sub-param.

### SDK Lifecycle

PICam SDK requires global initialization before any camera operations:

```
Picam_InitializeLibrary()
  → Picam_OpenFirstCamera() or Picam_OpenCamera()
  → Configure → Picam_CommitParameters()
  → Picam_Acquire() (loop)
  → Picam_CloseCamera()
Picam_UninitializeLibrary()
```

Use **static reference counting** (same pattern as qhyccd/hamamatsu plugins):
- First driver instance calls `Picam_InitializeLibrary()`
- Last driver instance calls `Picam_UninitializeLibrary()`
- Track with static `std::atomic<int> s_libraryRefCount`

### Enumeration String Mapping (Enumeration → StringCollection)

When a Picam parameter has `value_type == PicamValueType_Enumeration`:

1. Query the enumeration type: `Picam_GetParameterEnumeratedType()` → returns `PicamEnumeratedType` (e.g., `PicamEnumeratedType_AdcAnalogGain`)
2. Query available values: `Picam_GetParameterCollectionConstraint()` for the enum → `values_array` contains all valid integer values
3. For each integer value, resolve to string: `Picam_GetEnumerationString(type, value, &str)`
4. Store strings in `constraint.validValues` and use `ParameterType::StringCollection`

This ensures the UI shows human-readable strings ("Low", "Medium", "High") instead of raw integer IDs.

---

## ANTI-PATTERNS

- **DO NOT** hardcode parameter lists — PICam parameters vary per camera model; use `Picam_GetParameters()` at runtime
- **DO NOT** assume a parameter exists — always check `Picam_IsParameterRelevant()` before exposing it
- **DO NOT** call typed getter on wrong value type — always check `Picam_GetParameterValueType()` first, then dispatch
- **DO NOT** leak Picam-allocated memory — always call `Picam_DestroyRois()`, `Picam_DestroyPulses()`, `Picam_DestroyModulations()`, `Picam_DestroyString()`, `Picam_DestroyParameters()` after use
- **DO NOT** block the main thread — use `QThread` for acquisition loop; `Picam_Acquire()` is blocking
- **DO NOT** modify SDK headers in `driver-sdk/picam/` — vendor files
- **DO NOT** forget to commit — parameter changes are NOT applied until `Picam_CommitParameters()` is called
- **DO NOT** expose all 100+ PICam parameters — filter by relevance and categorize; too many parameters degrade UX
- **DO NOT** skip error checking — every PICam function returns `PicamError`; check against `PicamError_None`
