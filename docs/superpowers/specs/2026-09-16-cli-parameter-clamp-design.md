# CLI Parameter Clamp — Design Spec

**Date:** 2026-09-16
**Scope:** Make CLI parameter-setting paths (`--set name=value` and `--sequence configure` steps) auto-clamp/snap values into valid ranges with a warning, matching the GUI's silent-correction behavior.

## Goal

CLI currently passes user input straight to `driver->setParameter` with no validation. The GUI's widget layer (`ParameterWidgetFactory.cpp`) does `qBound` + `correctStep` on `editingFinished`, so users never see driver-side `ValueOutOfRange` rejections for typos or boundary mistakes. The CLI should provide the same forgiving behavior, plus a warning that surfaces what was changed so the user can spot errors.

## Behavioural Decisions (locked)

| Dimension | Decision |
|---|---|
| Behavior semantics | **Silent clamp + warning** (matches GUI). Driver keeps its hard-reject contract for values that survive clamping. |
| Coverage | Both `--set` flag and `--sequence configure` steps. |
| Cross-parameter validation | **Not handled at CLI layer.** Driver's `commitParameters` still owns ROI/binning consistency checks. |
| GUI widget-layer clamp | **Untouched.** GUI keeps its own per-widget clamp; CLI introduces its own commit-time clamp. The two layers operate at different points in the flow (input-time vs. submit-time). |
| Driver interface (`ICameraDriver`) | **Untouched.** No new virtual methods. |
| Unit conversion (ms ↔ s) | **Not added.** CLI users keep passing raw values (e.g. `exposure=500` means 500 ms). |
| Exit code semantics | **Unchanged.** Clamp warnings do **not** trigger non-zero exit. Silent correction, not failure. |
| Dead `--set` flag | **Fixed.** `opts.setParameters` is currently parsed but never consumed by `cli::run()`. Will now actually apply. |
| Logging destination | `qWarning` for changed/skipped; `qInfo` for success — same as existing CLI conventions. |

## Architecture

```
src/core/CameraTypes.h            Add inline clampValue() + ClampResult struct
                                       │  reuses validate() semantics
                                       ▼
src/cli/HeadlessController.cpp    runSequence() Configure branch uses clampValue()
                                  run() applies opts.setParameters via clampValue()
                                       ▲
                                       │
tests/test_cli_clamp/ (new)       Unit tests for clampValue() + integration tests
                                  through applyClampedSet() helper
```

`clampValue()` lives in `CameraTypes.h` next to the existing `validate()` /
`validateReason()` free functions so the canonical parameter-rule logic stays
in one header and can later be adopted by the GUI if desired (out of scope now).

## Components

### 1. `clampValue()` — `src/core/CameraTypes.h`

**New type:**

```cpp
struct ClampResult {
    QVariant value;
    bool changed;
    QString reason;
};
```

**New inline function:**

```cpp
inline ClampResult clampValue(const QVariant &raw,
                              const ParameterDefinition &def);
```

**Rules by `ParameterType`:**

| `ParameterType` | Rule | `changed` is true when |
|---|---|---|
| `FloatRange` | `qBound(min, raw.toDouble(), max)` → `correctStep` | value differs from original (clamp and/or step) |
| `IntRange`   | `qBound(min, raw.toInt(),    max)` → `correctStep` | same |
| `FloatCollection` / `IntCollection` / `StringCollection` | reject if `raw ∉ def.constraint.validValues` | n/a (rejection path) |
| `Boolean` | `raw.toBool()` | `raw.type() != QVariant::Bool` (lossy type conversion) |
| `String`  | `raw.toString()` | never (String has no validation) |

**Common gates (in this order, before type-specific logic):**

1. `def.name.isEmpty()` → `{QVariant(), false, "unknown parameter"}`
2. `def.isReadOnly`     → `{QVariant(), false, "parameter is read-only"}`
3. `!def.isValid()`     → `{QVariant(), false, "invalid parameter definition"}` (catches default-constructed `ParameterDefinition{}`)

**Step-snap formula** (mirrors `ParameterWidgetFactory.cpp:44-47`):

```cpp
correctStep(v, min, step) = min + qRound((v - min) / step) * step
```

**Reason strings (illustrative):**

- `"exposure: clamped from 99999.0 to 10000.0"`
- `"exposure: stepped from 50.7 to 50.0 (step=1)"`
- `"binning: invalid value 3; must be one of [1, 2, 4, 8]"`
- `"cooling_sensor_temp: parameter is read-only"`
- `"foo: unknown parameter"`

### 2. CLI integration — `src/cli/HeadlessController.cpp`

**Helper (new, anonymous namespace at top of file):**

```cpp
struct ApplyReport {
    int clamped = 0;   // parameters that were silently corrected
    int skipped = 0;   // parameters that were rejected by clampValue
    int setFailed = 0; // parameters that driver->setParameter refused
};

ApplyReport applyClampedSet(ICameraDriver *driver,
                            const QVariantMap &params,
                            const QString &indent)
{
    ApplyReport rep;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &raw  = it.value();
        ParameterDefinition def = driver->parameter(name);
        ClampResult r = clampValue(raw, def);
        if (r.changed) {
            qWarning().noquote() << indent + name + ": " + r.reason;
            ++rep.clamped;
        }
        if (!r.value.isValid()) {
            qWarning().noquote() << indent + "Skipped" << name;
            ++rep.skipped;
            continue;
        }
        qInfo().noquote() << indent + "set " + name + " = " + r.value.toString();
        if (!driver->setParameter(name, r.value)) {
            qWarning().noquote() << indent + "Failed to set " + name;
            ++rep.setFailed;
        }
    }
    QStringList commitFailed;
    driver->commitParameters(&commitFailed);
    if (!commitFailed.isEmpty())
        qWarning().noquote() << indent + "commitParameters rejected: "
                             + commitFailed.join(", ");
    return rep;
}
```

**A) `runSequence()` Configure branch** (`HeadlessController.cpp:122-127`):

```cpp
case SequenceStep::Configure:
    applyClampedSet(driver, step.parameters, QStringLiteral("  "));
    break;
```

**B) `--set` flag plumbing in `run()`** (new block, inserted between the existing initial-commit at line 185-190 and the sequence/capture branch at line 193):

```cpp
if (!opts.setParameters.isEmpty()) {
    qInfo() << "Applying" << opts.setParameters.size() << "--set parameters";
    applyClampedSet(driver, opts.setParameters, QString());
}
```

The two paths share the same `applyClampedSet` helper. The only difference is the leading indent (`"  "` for sequence steps, `""` for top-level `--set`).

### 3. Tests — new `tests/test_cli_clamp/` module

`tests/test_cli/` is DISABLED per `tests/AGENTS.md` (referenced types deleted in the prior merge). This is a fresh module, not a re-enable.

**Directory layout** (mirrors `tests/test_mock_driver/`):

```
tests/test_cli_clamp/
├── CMakeLists.txt
└── test_cli_clamp.cpp
```

**`CMakeLists.txt`** links Qt6::Test + Qt6::Core + `ezspec_core_lib` + mock driver plugin loader (to instantiate `MockCameraDriver` for integration tests).

**Unit tests** for `clampValue()` — no driver required, just construct `ParameterDefinition` literals:

| Test method | Asserts |
|---|---|
| `test_clamp_float_range_clamp_above_max` | raw=99999, FloatRange 1..10000 step 1 → `{10000, true, reason contains "clamped"}` |
| `test_clamp_float_range_clamp_below_min` | raw=0.5, FloatRange 1..10000 step 1 → `{1, true, ...}` |
| `test_clamp_float_range_step_align_above` | raw=50.7 → `{51, true, reason contains "stepped"}` |
| `test_clamp_float_range_step_align_below` | raw=50.3 → `{50, true, ...}` |
| `test_clamp_int_range_*` | parallel int variants |
| `test_clamp_int_collection_rejects_invalid` | raw=3, valid=[1,2,4,8] → `{invalid, false, reason contains "must be one of"}` |
| `test_clamp_float_collection_rejects_invalid` | same pattern |
| `test_clamp_string_collection_rejects_invalid` | same pattern |
| `test_clamp_collection_accepts_valid` | raw=4, valid=[1,2,4,8] → `{4, false, reason empty}` |
| `test_clamp_boolean_coerces_from_string` | raw="true" → `{true, true}` (lossy) |
| `test_clamp_boolean_unchanged_when_already_bool` | raw=true → `{true, false}` |
| `test_clamp_string_passthrough` | raw=any string → `{<same>, false}` |
| `test_clamp_readonly_rejects` | isReadOnly=true → `{invalid, ...}` |
| `test_clamp_unknown_param_rejects` | name="" → `{invalid, reason "unknown parameter"}` |
| `test_clamp_invalid_definition_rejects` | default `ParameterDefinition{}` → `{invalid, ...}` |
| `test_clamp_valid_value_unchanged` | raw=100, valid range → `{100, false}` |

**Integration tests** for `applyClampedSet()` — instantiate `MockCameraDriver`, exercise through helper:

| Test method | Asserts |
|---|---|
| `test_apply_clamp_above_max_corrects` | apply `{"exposure": 99999.0}` → driver `parameterValue("exposure") == 10000.0`; `rep.clamped == 1`; `rep.skipped == 0` |
| `test_apply_collection_rejected_is_skipped` | apply `{"binning": 3}` → driver `parameterValue("binning")` unchanged from default; `rep.skipped == 1`; `rep.clamped == 0` |
| `test_apply_unknown_param_is_skipped` | apply `{"nonexistent": 1}` → `rep.skipped == 1` |
| `test_apply_valid_value_no_change` | apply `{"exposure": 100.0}` → driver value == 100.0; `rep.clamped == 0`; `rep.skipped == 0` |

`qWarning`/`qInfo` output verification: install a `QtMessageHandler` shim that captures the stream, or just verify the `ApplyReport` counters (simpler — chosen).

**`tests/CMakeLists.txt`** adds `add_subdirectory(test_cli_clamp)`. Mock driver is always built so this module has no dependencies on optional SDKs.

### 4. Documentation updates

- `src/cli/README.md`:
  - `--set` description: append "Value outside the parameter's valid range is auto-clamped to the nearest valid value, with a warning printed to stderr."
  - `configure` step description: same sentence.
  - No change to examples.
- `src/core/CameraTypes.md`: add a `clampValue()` section under the existing `validate()` / `validateReason()` block.
- `ICameraDriver.md`: **not changed** (interface untouched).

## Error Handling Matrix

| Situation | CLI behavior | Exit code |
|---|---|---|
| Clamp no-op (value already valid) | `qInfo` "set x = v" | 0 |
| Clamp modified (clamp or step) | `qWarning` reason + `qInfo` "set x = v_corrected" | 0 |
| Clamp rejected (unknown / read-only / invalid collection) | `qWarning` reason + skip | 0 (other params still attempted) |
| `driver->setParameter` returns false | `qWarning` (existing) | 0 |
| `driver->commitParameters` fails | `qWarning` lists failed names (existing) | 0 |
| Cross-parameter invalid (ROI out of bounds) | not handled; driver rejects at commit | 0 |

The CLI exit code is **not** affected by clamping. Silent correction is not a failure.

## Data Flow

### `--set exposure=99999 --frames 1`

```
main.cpp:72-77        parse "exposure=99999" -> opts.setParameters["exposure"] = 99999.0
HeadlessController::run()
  driver->connectToCamera()       ok
  driver->commitParameters()      ok (empty batch, existing)
  applyClampedSet(opts.setParameters):
    name=exposure, raw=99999.0
    def = driver->parameter("exposure")  -> FloatRange 1..10000 step 1
    r   = clampValue(99999.0, def)       -> {10000.0, true, "clamped from 99999.0 to 10000.0"}
    qWarning "exposure: clamped from 99999.0 to 10000.0"
    qInfo    "set exposure = 10000"
    driver->setParameter("exposure", 10000)  -> true
    driver->commitParameters()               -> ok
  captureFrames(driver, 1, ...)
```

### `--sequence config.json` with `{"configure": {"gain": 5.5}}`

```
SequenceRunner::loadFromFile() -> step.parameters["gain"] = 5.5
runSequence() -> Configure step
  applyClampedSet(driver, {"gain": 5.5}, "  "):
    def = driver->parameter("gain")  -> FloatRange 0..40 step 0.1
    r   = clampValue(5.5, def)        -> {5.5, false, ""}
    qInfo    "  set gain = 5.5"
    driver->setParameter("gain", 5.5)  -> true
    driver->commitParameters()         -> ok
```

## Out of Scope

Explicitly **not done** in this spec:

- GUI widget-layer refactor to use `clampValue()` (could happen later)
- Driver-layer changes (`QHYCCDDriver`, `HamamatsuDriver`, `MockCameraDriver` untouched)
- `ICameraDriver` interface additions
- Cross-parameter validation (ROI bounds, binning-coupled ranges)
- Unit auto-conversion in CLI (users keep passing raw values)
- `--strict` mode flag (rejects instead of clamps)
- Any change to existing exit-code semantics
- Transactional rollback if `commitParameters` rejects some parameters
- Re-enabling / replacing the disabled `tests/test_cli/` directory

## Test Strategy

- **Unit:** 16 `clampValue()` cases cover every `ParameterType`, every common gate, and step-snap rounding direction.
- **Integration:** 4 `applyClampedSet()` cases verify driver state actually changes after clamp.
- **Regression:** no existing tests modified. `test_mock_driver`, `test_app_controller`, `test_qhyccd_driver`, etc. untouched.
- **Manual smoke:** `.\build_preset.bat debug` then `ezspeccam --camera mock-001 --set exposure=99999 --frames 1` should print one `qWarning` line and capture with `exposure = 10000`.

## Implementation Plan Reference

Plan to be generated by `writing-plans` skill after spec approval.