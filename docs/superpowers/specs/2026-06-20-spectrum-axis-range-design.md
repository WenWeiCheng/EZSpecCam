# Spectrum View Axis Range — Design Spec

**Date:** 2026-06-20
**Scope:** `SpectrumViewWidget` X/Y axis range control, mirroring `ImageViewWidget`'s zoom/reset pattern.

## Goal

Replace the current six-value `XAxisRangeMode` (`Auto`, `Full`, `ZoomLeft`, `ZoomRight`, `ZoomCenter`, `Custom`) in `SpectrumViewWidget` with a unified **Auto / Manual** mode that applies independently to the X and Y axes. The existing left-drag zoom and right-click reset behaviour must be preserved, and the right-click reset must restore the range that the current mode dictates rather than always snapping back to a fixed full range.

## Background

The current state of `src/gui/widgets/display/SpectrumViewWidget.{h,cpp}`:

- The enum `XAxisRangeMode` exposes six modes that have no UI bindings: only `Auto` is the default, and the rest are unreachable through `MainWindow`.
- `CustomRangeDialog` exists at `src/gui/widgets/dialogs/CustomRangeDialog.{h,cpp}` but is only `#include`d from `MainWindow.cpp` and never instantiated. It is dead code.
- Left-drag zoom sets `m_userHasZoomed = true`; right-click calls `resetZoomToFit()` which delegates to `applyXAxisRange()`. `applyXAxisRange()` only touches `xAxis->setRange()` and leaves the Y axis untouched.
- New data via `setData()` calls `applyXAxisRange()` only when `m_userHasZoomed == false`. Y axis never rescales to data on its own.

`ImageViewWidget` already has the requested behaviour for the 2D case: left-drag zoom keeps user range, right-click resets to image bounds. We extend that pattern to both axes of the spectrum plot.

## Behaviour Matrix

| X Mode | Y Mode | After left-drag zoom | Right-click resets to |
|--------|--------|----------------------|-----------------------|
| Auto   | Auto   | `m_userHasZoomed = true`; X+Y frozen to zoomed rect | X auto-fit to data, Y auto-fit to data |
| Auto   | Manual | same | X auto-fit to data, Y = manual range |
| Manual | Auto   | same | X = manual range, Y auto-fit to data |
| Manual | Manual | same | X = manual range, Y = manual range |
| (zoomed, any combination of modes) | — | subsequent zoom operates inside current visible rect | restore current mode defaults |

New data arrival keeps the zoomed range (`m_userHasZoomed == true`); only when not zoomed does `setData()` apply the mode defaults.

## Component Changes

### `SpectrumViewWidget.h`

Replace the X-only enum and accessor surface with a unified `AxisRangeMode`:

```cpp
enum class AxisRangeMode { Auto, Manual };
Q_ENUM(AxisRangeMode)
```

Public API:

- `void setXAxisRangeMode(AxisRangeMode mode);`
- `void setYAxisRangeMode(AxisRangeMode mode);`
- `AxisRangeMode xAxisRangeMode() const;`
- `AxisRangeMode yAxisRangeMode() const;`
- `void setManualXRange(double min, double max);`
- `void setManualYRange(double min, double max);`
- `double manualXMin() const; double manualXMax() const;`
- `double manualYMin() const; double manualYMax() const;`
- `double currentXMin() const; double currentXMax() const;`  *(kept)*
- `double currentYMin() const; double currentYMax() const;`  *(new)*

Remove:

- `enum class XAxisRangeMode` and its six values
- `void setXAxisRangeMode(XAxisRangeMode)`
- `void setCustomXRange(double, double)`
- `double customXMin() const; double customXMax() const;`

Members:

```cpp
AxisRangeMode m_xAxisRangeMode = AxisRangeMode::Auto;
AxisRangeMode m_yAxisRangeMode = AxisRangeMode::Auto;
double m_manualXMin = 0.0;
double m_manualXMax = 100.0;
double m_manualYMin = 0.0;
double m_manualYMax = 100.0;
```

Private helpers:

- `void applyAxisRange();`  *(replaces `applyXAxisRange`)*

### `SpectrumViewWidget.cpp`

`applyAxisRange()` computes and applies X and Y ranges together:

- `m_userHasZoomed == true` → return immediately (preserves zoom).
- For each axis independently:
  - `Auto` → derive bounds from current data (`m_xData`, `m_yData`); apply 2% padding for X (matches existing behaviour) and a padding that is the larger of `(max-min)*0.02` and `1.0` for Y.
  - `Manual` → use the corresponding `m_manualXMin/Max` or `m_manualYMin/Max`.

`resetZoomToFit()` clears `m_userHasZoomed` and calls `applyAxisRange()`.

`setData()`, `setSpectrumData()`, `setFromImage()`, and `setIntensityScaleType()` all switch to `applyAxisRange()` instead of `applyXAxisRange()`. The auto-fit branch now rescales both X and Y.

`eventFilter()` already calls `m_plot->xAxis->setRange()` and `m_plot->yAxis->setRange()` on rubber-band release; this is unchanged.

`clearData()` resets axis ranges to (0, 100) for both axes and hides the cursor. Zoom flag is reset to `false`.

### `ScaleControlDialog.{h,cpp}`

Append controls to the existing `m_spectrumGroup` (do not add a new group box):

| Row | Control | Behaviour |
|-----|---------|-----------|
| X Range | `QComboBox` | `Auto` (data 0) / `Manual` (data 1) |
| Y Range | `QComboBox` | `Auto` (data 0) / `Manual` (data 1) |
| X Min / X Max | `QDoubleSpinBox` × 2 | Enabled iff X Range = Manual; range `[-1e6, 1e6]`, decimals 2 |
| Y Min / Y Max | `QDoubleSpinBox` × 2 | Enabled iff Y Range = Manual; range `[-1e6, 1e6]`, decimals 2 |

New signals:

```cpp
void spectrumXRangeModeChanged(int mode);      // 0 = Auto, 1 = Manual
void spectrumYRangeModeChanged(int mode);
void spectrumManualXRangeChanged(double min, double max);
void spectrumManualYRangeChanged(double min, double max);
```

New public API:

```cpp
void setSpectrumXRangeMode(int mode);
void setSpectrumYRangeMode(int mode);
int  spectrumXRangeMode() const;
int  spectrumYRangeMode() const;
void setSpectrumManualXRange(double min, double max);
void setSpectrumManualYRange(double min, double max);
```

Spin-box enable/disable is updated whenever the corresponding combo changes. Both spin boxes emit on `editingFinished`. Min must be strictly less than max; if the user types an invalid pair, the OK path is blocked via `QDoubleSpinBox::setMinimum()` set on the max box to track the current min value (and vice versa). Block signals around programmatic updates to avoid feedback loops.

### `MainWindow.cpp`

In `MainWindow` constructor, after the existing spectrum wiring, add four connections that forward dialog signals to `m_spectrumViewWidget`:

```cpp
connect(m_scaleDialog, &ScaleControlDialog::spectrumXRangeModeChanged,
        this, [this](int mode) {
            m_spectrumViewWidget->setXAxisRangeMode(
                mode == 0 ? SpectrumViewWidget::AxisRangeMode::Auto
                          : SpectrumViewWidget::AxisRangeMode::Manual);
        });

connect(m_scaleDialog, &ScaleControlDialog::spectrumYRangeModeChanged,
        this, [this](int mode) {
            m_spectrumViewWidget->setYAxisRangeMode(
                mode == 0 ? SpectrumViewWidget::AxisRangeMode::Auto
                          : SpectrumViewWidget::AxisRangeMode::Manual);
        });

connect(m_scaleDialog, &ScaleControlDialog::spectrumManualXRangeChanged,
        this, [this](double min, double max) {
            m_spectrumViewWidget->setManualXRange(min, max);
        });

connect(m_scaleDialog, &ScaleControlDialog::spectrumManualYRangeChanged,
        this, [this](double min, double max) {
            m_spectrumViewWidget->setManualYRange(min, max);
        });
```

Initial state: call `m_scaleDialog->setSpectrumXRangeMode(0)` and `setSpectrumYRangeMode(0)` once after the dialog is constructed (matching the current `setSpectrumScaleType(0)` pattern).

Remove `#include "dialogs/CustomRangeDialog.h"` from `MainWindow.cpp`.

### Removal of `CustomRangeDialog`

Delete the following files:

- `src/gui/widgets/dialogs/CustomRangeDialog.h`
- `src/gui/widgets/dialogs/CustomRangeDialog.cpp`

Remove the corresponding entries from `src/gui/CMakeLists.txt`.

## Testing

Add a new test module `tests/test_spectrum_axis_range/` with a single executable `test_spectrum_axis_range`. Uses Qt Test (matching the rest of the suite). Cases:

1. `test_default_modes_are_auto` — both axes default to `Auto`; Y range tracks data after `setData`.
2. `test_manual_x_range_applies_immediately` — `setManualXRange(10, 50)` makes `currentXMin/Max()` equal (10, 50) regardless of data.
3. `test_manual_y_range_applies_immediately` — symmetric for Y.
4. `test_right_click_resets_auto_to_data_bounds` — switch to Manual, zoom via `resetZoomToFit` path, then re-arm Auto and call `resetZoomToFit`; X and Y both fit the data.
5. `test_right_click_resets_manual_to_user_range` — Manual mode + zoom + `resetZoomToFit` returns to the manual X/Y.
6. `test_right_click_mixed_modes` — X=Manual Y=Auto; `resetZoomToFit` keeps manual X and refits Y to data.
7. `test_new_data_preserves_zoom` — zoom first, then `setData()`; `currentXMin/Max` unchanged.
8. `test_new_data_rescales_when_not_zoomed` — not zoomed; new data with different y range refits Y.
9. `test_axis_modes_independent` — `setXAxisRangeMode(Manual)` does not flip `yAxisRangeMode()`.

Each test creates a local `SpectrumViewWidget`, calls `setData()` with synthetic vectors, and inspects the public `currentX*` / `currentY*` accessors. To exercise the right-click reset path from tests, expose a public wrapper `void resetZoom()` on the widget that delegates to the existing private `resetZoomToFit()`. Tests call `resetZoom()` directly after manipulating the widget into a zoomed state by setting `m_userHasZoomed` indirectly: the simplest reliable trigger is `setManualXRange` followed by a `resetZoom()` call, which validates that the right-click path restores the configured manual values. To create a true "zoomed" state for negative tests, expose a `void setZoomedForTest(bool)` helper that flips `m_userHasZoomed` and is only compiled in the test build via `#ifdef SPECTRUM_AXIS_RANGE_TESTING`; this keeps the production surface small.

## File Manifest

| File | Change |
|------|--------|
| `src/gui/widgets/display/SpectrumViewWidget.h` | Replace enum, update API, members |
| `src/gui/widgets/display/SpectrumViewWidget.cpp` | New `applyAxisRange`, updated `resetZoomToFit`, all callers |
| `src/gui/widgets/dialogs/ScaleControlDialog.h` | New members + signals |
| `src/gui/widgets/dialogs/ScaleControlDialog.cpp` | New combo/spinBox wiring |
| `src/gui/widgets/dialogs/CustomRangeDialog.h` | **Delete** |
| `src/gui/widgets/dialogs/CustomRangeDialog.cpp` | **Delete** |
| `src/gui/CMakeLists.txt` | Remove CustomRangeDialog entries |
| `src/gui/widgets/MainWindow.cpp` | Remove CustomRangeDialog include; add four connections; initial dialog state |
| `tests/test_spectrum_axis_range/CMakeLists.txt` | New |
| `tests/test_spectrum_axis_range/test_spectrum_axis_range.cpp` | New test cases |
| `tests/CMakeLists.txt` | Add subdirectory |

## Risks and Mitigations

- **ProfileWindow consumers** — `ProfileWindow.cpp` only uses `SpectrumViewWidget` via the standard zoom/cursor API; it never calls `setXAxisRangeMode` or `setCustomXRange`. The removal of those methods is safe.
- **Backward API removal** — no external plugin or worker touches the removed API (verified via grep across `src/`).
- **Logarithmic Y axis** — `setIntensityScaleType(Log)` swaps the Y ticker but does not change the range-setting path; `applyAxisRange()` still calls `m_plot->yAxis->setRange()` after the swap. Manual Y values that include ≤0 in Log mode are user error and out of scope; the spin-box range warning is sufficient.
- **Manual range smaller than data** — by design; manual overrides everything until reset.

## Out of Scope

- Persisting the chosen mode across sessions via `QSettings`.
- Programmatic zoom from outside the widget.
- Modifying `ProfileWindow`'s `SpectrumViewWidget` instances.