# Spectrum Axis Range Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the six-value `XAxisRangeMode` in `SpectrumViewWidget` with a unified `Auto`/`Manual` mode that applies independently to the X and Y axes, and expose the controls through `ScaleControlDialog`.

**Architecture:** Introduce a single `AxisRangeMode` enum and symmetric X/Y state on the widget. A new `applyAxisRange()` private method computes both axes from current data (Auto) or user-supplied manual bounds (Manual), honoring the existing `m_userHasZoomed` flag. `ScaleControlDialog`'s Spectrum group grows two combo boxes plus four spin boxes; their signals are wired in `MainWindow` to the widget. The unused `CustomRangeDialog` is deleted.

**Tech Stack:** C++17, Qt 6.8 (Widgets, Test), QCustomPlot (read-only), CMake.

**Reference Spec:** `docs/superpowers/specs/2026-06-20-spectrum-axis-range-design.md`

**Working Directory:** `D:\10_Projects\2502-Sw-EZSpecCam-shadow`

---

## File Structure

| File | Responsibility |
|------|----------------|
| `src/gui/widgets/display/SpectrumViewWidget.h` | Public API + state for axis range modes |
| `src/gui/widgets/display/SpectrumViewWidget.cpp` | `applyAxisRange` logic, rubber-band zoom integration |
| `src/gui/widgets/dialogs/ScaleControlDialog.h` | New members + signals |
| `src/gui/widgets/dialogs/ScaleControlDialog.cpp` | New combo/spinBox wiring |
| `src/gui/widgets/MainWindow.cpp` | Connect dialog signals to widget; remove dead include |
| `src/gui/CMakeLists.txt` | Drop CustomRangeDialog sources |
| `tests/test_spectrum_axis_range/CMakeLists.txt` | New test module build |
| `tests/test_spectrum_axis_range/test_spectrum_axis_range.cpp` | New tests |
| `tests/CMakeLists.txt` | Register new test module |
| `src/gui/widgets/dialogs/CustomRangeDialog.{h,cpp}` | **Deleted** |

---

## Task 1: Add failing tests for SpectrumViewWidget axis range

**Files:**
- Create: `tests/test_spectrum_axis_range/CMakeLists.txt`
- Create: `tests/test_spectrum_axis_range/test_spectrum_axis_range.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Create `tests/test_spectrum_axis_range/CMakeLists.txt`**

Write the following content:

```cmake
# Axis range behaviour tests for SpectrumViewWidget
add_executable(test_spectrum_axis_range)
target_sources(test_spectrum_axis_range PRIVATE
    test_spectrum_axis_range.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/SpectrumViewWidget.cpp
)
target_link_libraries(test_spectrum_axis_range PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
target_compile_definitions(test_spectrum_axis_range PRIVATE
    QCUSTOMPLOT_USE_OPENGL
    SPECTRUM_AXIS_RANGE_TESTING
)
target_include_directories(test_spectrum_axis_range PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
set_target_properties(test_spectrum_axis_range PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_spectrum_axis_range COMMAND test_spectrum_axis_range)
```

- [ ] **Step 2: Create `tests/test_spectrum_axis_range/test_spectrum_axis_range.cpp`**

Write the following content:

```cpp
#include <QCoreApplication>
#include <QObject>
#include <QTest>
#include <QVector>
#include "SpectrumViewWidget.h"

class TestSpectrumAxisRange : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}
    void init() {}
    void cleanup() {}

    void test_default_modes_are_auto()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i * 10.0;
        }
        widget.setData(x, y);

        QCOMPARE(widget.xAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Auto);
        QCOMPARE(widget.yAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Auto);

        QCOMPARE(widget.currentXMin(), -2.0);
        QCOMPARE(widget.currentXMax(), 102.0);
        QCOMPARE(widget.currentYMin(), -2.0);
        QCOMPARE(widget.currentYMax(), 1002.0);
    }

    void test_manual_x_range_applies()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setManualXRange(10.0, 50.0);

        QCOMPARE(widget.currentXMin(), 10.0);
        QCOMPARE(widget.currentXMax(), 50.0);
        QCOMPARE(widget.currentYMin(), -2.0);
        QCOMPARE(widget.currentYMax(), 102.0);
    }

    void test_manual_y_range_applies()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setManualYRange(0.0, 1000.0);

        QCOMPARE(widget.currentYMin(), 0.0);
        QCOMPARE(widget.currentYMax(), 1000.0);
        QCOMPARE(widget.currentXMin(), -2.0);
        QCOMPARE(widget.currentXMax(), 102.0);
    }

    void test_right_click_resets_manual()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setManualXRange(20.0, 80.0);
        widget.setManualYRange(-5.0, 50.0);

        widget.setZoomedForTest(true);
        widget.resetZoom();

        QCOMPARE(widget.currentXMin(), 20.0);
        QCOMPARE(widget.currentXMax(), 80.0);
        QCOMPARE(widget.currentYMin(), -5.0);
        QCOMPARE(widget.currentYMax(), 50.0);
    }

    void test_right_click_resets_auto_to_data_bounds()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setManualXRange(20.0, 80.0);
        widget.setManualYRange(-5.0, 50.0);
        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);

        widget.setZoomedForTest(true);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Auto);
        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Auto);

        widget.resetZoom();

        QCOMPARE(widget.currentXMin(), -2.0);
        QCOMPARE(widget.currentXMax(), 102.0);
        QCOMPARE(widget.currentYMin(), -2.0);
        QCOMPARE(widget.currentYMax(), 102.0);
    }

    void test_right_click_mixed_modes()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Auto);
        widget.setManualXRange(10.0, 90.0);

        widget.setZoomedForTest(true);
        widget.resetZoom();

        QCOMPARE(widget.currentXMin(), 10.0);
        QCOMPARE(widget.currentXMax(), 90.0);
        QCOMPARE(widget.currentYMin(), -2.0);
        QCOMPARE(widget.currentYMax(), 102.0);
    }

    void test_new_data_preserves_zoom()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setZoomedForTest(true);
        widget.setManualXRange(20.0, 60.0);
        widget.setManualYRange(10.0, 70.0);

        QVector<double> y2(100);
        for (int i = 0; i < 100; ++i) {
            y2[i] = (i + 500) * 1.0;
        }
        widget.setData(x, y2);

        QCOMPARE(widget.currentXMin(), 20.0);
        QCOMPARE(widget.currentXMax(), 60.0);
        QCOMPARE(widget.currentYMin(), 10.0);
        QCOMPARE(widget.currentYMax(), 70.0);
    }

    void test_new_data_rescales_when_not_zoomed()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        QVector<double> y2(100);
        for (int i = 0; i < 100; ++i) {
            y2[i] = (i + 500) * 1.0;
        }
        widget.setData(x, y2);

        QCOMPARE(widget.currentYMin(), 498.0);
        QCOMPARE(widget.currentYMax(), 602.0);
    }

    void test_axis_modes_independent()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(10), y(10);
        for (int i = 0; i < 10; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        QCOMPARE(widget.yAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Auto);

        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        QCOMPARE(widget.xAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Manual);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Auto);
        QCOMPARE(widget.yAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Manual);
    }
};

QTEST_MAIN(TestSpectrumAxisRange)
#include "test_spectrum_axis_range.moc"
```

- [ ] **Step 3: Register the new module in `tests/CMakeLists.txt`**

Edit `tests/CMakeLists.txt`. Add one line in the existing alphabetical-ish list (keep order stable):

```cmake
add_subdirectory(test_spectrum_axis_range)
```

Insert it after `add_subdirectory(test_row_range_dialog)` (currently line 13) and before `add_subdirectory(test_spectrum_auto_recovery)`.

- [ ] **Step 4: Configure + build to confirm tests fail to compile**

Run from the workspace root:

```bash
.\build_preset.bat debug 2>&1 | Select-String -Pattern "error|Error" | Select-Object -First 30
```

Expected: compiler errors referencing `AxisRangeMode`, `setXAxisRangeMode`, `setManualXRange`, `currentYMin/Max`, `setZoomedForTest`, `resetZoom`. These do not exist yet — that is correct. Capture the error list to confirm it covers every missing symbol.

---

## Task 2: Update SpectrumViewWidget.h — replace enum and add API

**Files:**
- Modify: `src/gui/widgets/display/SpectrumViewWidget.h`

- [ ] **Step 1: Replace the `XAxisRangeMode` enum and update the public API**

Edit `src/gui/widgets/display/SpectrumViewWidget.h`. Make these exact changes:

1. Replace the existing `enum class XAxisRangeMode` block (currently lines 22–30) with:

```cpp
    enum class AxisRangeMode {
        Auto,
        Manual
    };
    Q_ENUM(AxisRangeMode)
```

2. Replace the existing range-related declarations (currently lines 65–73) with:

```cpp
    AxisRangeMode xAxisRangeMode() const { return m_xAxisRangeMode; }
    void setXAxisRangeMode(AxisRangeMode mode);

    AxisRangeMode yAxisRangeMode() const { return m_yAxisRangeMode; }
    void setYAxisRangeMode(AxisRangeMode mode);

    void setManualXRange(double min, double max);
    void setManualYRange(double min, double max);

    double manualXMin() const { return m_manualXMin; }
    double manualXMax() const { return m_manualXMax; }
    double manualYMin() const { return m_manualYMin; }
    double manualYMax() const { return m_manualYMax; }

    int dataWidth() const { return m_xData.isEmpty() ? 0 : m_xData.size(); }

    double currentXMin() const { return m_plot->xAxis->range().lower; }
    double currentXMax() const { return m_plot->xAxis->range().upper; }
    double currentYMin() const { return m_plot->yAxis->range().lower; }
    double currentYMax() const { return m_plot->yAxis->range().upper; }

    void resetZoom();
#ifdef SPECTRUM_AXIS_RANGE_TESTING
    void setZoomedForTest(bool zoomed) { m_userHasZoomed = zoomed; }
#endif
```

3. Replace the private helper declaration `void applyXAxisRange();` (currently line 95) with:

```cpp
    void applyAxisRange();
```

4. Replace the existing member block (currently lines 105–111) with:

```cpp
    XAxisRangeModePlaceholder
```

Wait — that is a placeholder. Use this exact replacement instead:

```cpp
    AxisRangeMode m_xAxisRangeMode = AxisRangeMode::Auto;
    AxisRangeMode m_yAxisRangeMode = AxisRangeMode::Auto;
    double m_manualXMin = 0.0;
    double m_manualXMax = 100.0;
    double m_manualYMin = 0.0;
    double m_manualYMax = 100.0;
```

- [ ] **Step 2: Confirm the header compiles cleanly**

Run:

```bash
.\build_preset.bat debug 2>&1 | Select-String -Pattern "SpectrumViewWidget\.h" | Select-Object -First 10
```

Expected: errors only inside `SpectrumViewWidget.cpp` (which still references the old API). No errors originating in the header.

- [ ] **Step 3: Commit the header update**

```bash
git add src/gui/widgets/display/SpectrumViewWidget.h
git commit -m "refactor(spectrum): replace XAxisRangeMode with AxisRangeMode (Auto/Manual)"
```

---

## Task 3: Update SpectrumViewWidget.cpp — implement applyAxisRange and friends

**Files:**
- Modify: `src/gui/widgets/display/SpectrumViewWidget.cpp`

- [ ] **Step 1: Rewrite `applyXAxisRange` as `applyAxisRange`**

Edit `src/gui/widgets/display/SpectrumViewWidget.cpp`. Replace the entire `applyXAxisRange()` method (currently lines 300–336) with:

```cpp
void SpectrumViewWidget::applyAxisRange()
{
    if (!m_dataValid || m_xData.isEmpty()) {
        return;
    }

    if (m_userHasZoomed) {
        return;
    }

    double minX = m_xData.first();
    double maxX = m_xData.last();
    double rangeX = maxX - minX;

    if (m_xAxisRangeMode == AxisRangeMode::Auto) {
        m_plot->xAxis->setRange(minX - rangeX * 0.02, maxX + rangeX * 0.02);
    } else {
        m_plot->xAxis->setRange(m_manualXMin, m_manualXMax);
    }

    if (m_yAxisRangeMode == AxisRangeMode::Auto) {
        double minY = *std::min_element(m_yData.constBegin(), m_yData.constEnd());
        double maxY = *std::max_element(m_yData.constBegin(), m_yData.constEnd());
        double yPadding = (maxY - minY) * 0.02;
        if (yPadding < 1.0) {
            yPadding = 1.0;
        }
        m_plot->yAxis->setRange(minY - yPadding, maxY + yPadding);
    } else {
        m_plot->yAxis->setRange(m_manualYMin, m_manualYMax);
    }
}
```

- [ ] **Step 2: Update `resetZoomToFit` and add public `resetZoom`**

In the same file:

1. Add `#include <cmath>` near the top (after `#include <limits>`) so the helper math is explicit (not strictly required but keeps the file consistent).

2. Replace the `resetZoomToFit()` definition (currently lines 518–527) with:

```cpp
void SpectrumViewWidget::resetZoom()
{
    if (!m_dataValid || m_xData.isEmpty()) {
        return;
    }

    m_userHasZoomed = false;
    applyAxisRange();
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}
```

3. Replace every call site of `resetZoomToFit()` with `resetZoom()`. The call site is inside `eventFilter()` (currently line 401). Update it:

```cpp
            } else if (me->button() == Qt::RightButton) {
                resetZoom();
                return true;
            }
```

4. Remove the old `void SpectrumViewWidget::resetZoomToFit()` definition entirely (the rewrite above replaces it).

- [ ] **Step 3: Replace `setXAxisRangeMode`, remove `setCustomXRange`, add Y/mode setters and `setManualXRange`/`setManualYRange`**

In the same file:

1. Replace the existing `setXAxisRangeMode` method (currently lines 245–258) with:

```cpp
void SpectrumViewWidget::setXAxisRangeMode(AxisRangeMode mode)
{
    if (m_xAxisRangeMode == mode) {
        return;
    }

    m_xAxisRangeMode = mode;
    m_userHasZoomed = false;

    if (m_dataValid && !m_xData.isEmpty()) {
        applyAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SpectrumViewWidget::setYAxisRangeMode(AxisRangeMode mode)
{
    if (m_yAxisRangeMode == mode) {
        return;
    }

    m_yAxisRangeMode = mode;
    m_userHasZoomed = false;

    if (m_dataValid && !m_xData.isEmpty()) {
        applyAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}
```

2. Replace the existing `setCustomXRange` (currently lines 260–269) with:

```cpp
void SpectrumViewWidget::setManualXRange(double min, double max)
{
    if (max <= min) {
        return;
    }

    m_manualXMin = min;
    m_manualXMax = max;

    if (m_xAxisRangeMode == AxisRangeMode::Manual && m_dataValid) {
        applyAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void SpectrumViewWidget::setManualYRange(double min, double max)
{
    if (max <= min) {
        return;
    }

    m_manualYMin = min;
    m_manualYMax = max;

    if (m_yAxisRangeMode == AxisRangeMode::Manual && m_dataValid) {
        applyAxisRange();
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}
```

- [ ] **Step 4: Update remaining call sites of `applyXAxisRange`**

Find every reference to `applyXAxisRange` in `SpectrumViewWidget.cpp`. There are four call sites (currently lines 101, 255, 266, 295). Replace each with `applyAxisRange`. Specifically:

1. In `setData()` (around line 101): change `applyXAxisRange();` to `applyAxisRange();`.
2. In `setIntensityScaleType()` (around line 295): change `applyXAxisRange();` to `applyAxisRange();`.
3. The references inside `setXAxisRangeMode` and `setManualXRange` were rewritten in Step 3 — verify they say `applyAxisRange()`.

- [ ] **Step 5: Update `clearData()` to reset both axes**

Edit the existing `clearData()` method (currently lines 164–178). Replace the body with:

```cpp
void SpectrumViewWidget::clearData()
{
    m_xData.clear();
    m_yData.clear();
    m_dataValid = false;

    m_graph->data()->clear();
    m_plot->xAxis->setRange(0, 100);
    m_plot->yAxis->setRange(0, 100);
    m_userHasZoomed = false;

    m_cursorLine->setVisible(false);
    m_cursorLabel->setVisible(false);

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}
```

- [ ] **Step 6: Build and verify only test failures remain**

Run:

```bash
.\build_preset.bat debug 2>&1 | Select-String -Pattern "error|Error" | Select-Object -First 30
```

Expected: zero compile errors in the GUI; the only failures should be inside `test_spectrum_axis_range.cpp` if any test logic was wrong (it should pass now). If any GUI compile error remains, fix it before continuing.

- [ ] **Step 7: Run the new test module**

Run:

```bash
& "build\msvc-debug\bin\Debug\test_spectrum_axis_range.exe" -v2 -o test_axis_range.txt 2>&1; Get-Content test_axis_range.txt -Tail 80
```

Expected: all 9 tests pass (`PASS : TestSpectrumAxisRange::test_*`).

- [ ] **Step 8: Commit the widget changes**

```bash
git add src/gui/widgets/display/SpectrumViewWidget.cpp
git commit -m "feat(spectrum): implement Auto/Manual axis range with reset"
```

---

## Task 4: Extend ScaleControlDialog.h — new members and signals

**Files:**
- Modify: `src/gui/widgets/dialogs/ScaleControlDialog.h`

- [ ] **Step 1: Add new includes**

Edit `src/gui/widgets/dialogs/ScaleControlDialog.h`. Replace the existing include block (lines 1–8) with:

```cpp
#ifndef SCALECONTROLDIALOG_H
#define SCALECONTROLDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
```

- [ ] **Step 2: Extend the public API**

Inside the class declaration, after `void setSpectrumScaleType(int type);` (currently line 20), add:

```cpp
    void setSpectrumXRangeMode(int mode);
    void setSpectrumYRangeMode(int mode);

    int spectrumXRangeMode() const;
    int spectrumYRangeMode() const;

    void setSpectrumManualXRange(double min, double max);
    void setSpectrumManualYRange(double min, double max);
```

After `int spectrumScaleType() const;` (currently line 24), add:

```cpp
    double spectrumManualXMin() const;
    double spectrumManualXMax() const;
    double spectrumManualYMin() const;
    double spectrumManualYMax() const;
```

- [ ] **Step 3: Add new signals**

Replace the existing `signals:` block (currently lines 26–29) with:

```cpp
signals:
    void imageScaleTypeChanged(int type);
    void imageColorScaleModeChanged(int mode);
    void spectrumScaleTypeChanged(int type);
    void spectrumXRangeModeChanged(int mode);
    void spectrumYRangeModeChanged(int mode);
    void spectrumManualXRangeChanged(double min, double max);
    void spectrumManualYRangeChanged(double min, double max);
```

- [ ] **Step 4: Add new private slots**

Replace the existing `private slots:` block (currently lines 31–34) with:

```cpp
private slots:
    void onImageScaleTypeChanged(int index);
    void onImageColorScaleModeChanged(int index);
    void onSpectrumScaleTypeChanged(int index);
    void onSpectrumXRangeModeChanged(int index);
    void onSpectrumYRangeModeChanged(int index);
    void onSpectrumManualXMinChanged();
    void onSpectrumManualXMaxChanged();
    void onSpectrumManualYMinChanged();
    void onSpectrumManualYMaxChanged();
```

- [ ] **Step 5: Add new private members**

Replace the existing `private:` members block (currently lines 36–46) with:

```cpp
private:
    void createImageGroup();
    void createSpectrumGroup();
    void updateSpectrumManualXEnabled();
    void updateSpectrumManualYEnabled();

    QGroupBox *m_imageGroup;
    QGroupBox *m_spectrumGroup;

    QComboBox *m_imageScaleTypeCombo;
    QComboBox *m_imageColorScaleModeCombo;
    QComboBox *m_spectrumScaleTypeCombo;
    QComboBox *m_spectrumXRangeModeCombo;
    QComboBox *m_spectrumYRangeModeCombo;
    QDoubleSpinBox *m_spectrumManualXMinSpin;
    QDoubleSpinBox *m_spectrumManualXMaxSpin;
    QDoubleSpinBox *m_spectrumManualYMinSpin;
    QDoubleSpinBox *m_spectrumManualYMaxSpin;
};
```

- [ ] **Step 6: Confirm header compiles**

```bash
.\build_preset.bat debug 2>&1 | Select-String -Pattern "ScaleControlDialog" | Select-Object -First 10
```

Expected: errors only inside `ScaleControlDialog.cpp` (it still lacks implementations).

- [ ] **Step 7: Commit the header**

```bash
git add src/gui/widgets/dialogs/ScaleControlDialog.h
git commit -m "feat(scale-dialog): declare spectrum X/Y range mode controls"
```

---

## Task 5: Extend ScaleControlDialog.cpp — wire up controls

**Files:**
- Modify: `src/gui/widgets/dialogs/ScaleControlDialog.cpp`

- [ ] **Step 1: Add include for spin box header**

Edit `src/gui/widgets/dialogs/ScaleControlDialog.cpp`. Add the include line right after the existing `#include <QLabel>` (currently line 6):

```cpp
#include <QDoubleSpinBox>
```

- [ ] **Step 2: Replace `createSpectrumGroup()` with the extended version**

Replace the existing `createSpectrumGroup()` (currently lines 52–65) with:

```cpp
void ScaleControlDialog::createSpectrumGroup()
{
    m_spectrumGroup = new QGroupBox("Spectrum Scale", this);

    QFormLayout *layout = new QFormLayout(m_spectrumGroup);

    m_spectrumScaleTypeCombo = new QComboBox(this);
    m_spectrumScaleTypeCombo->addItem("linear", 0);
    m_spectrumScaleTypeCombo->addItem("log", 1);
    layout->addRow("Type:", m_spectrumScaleTypeCombo);

    m_spectrumXRangeModeCombo = new QComboBox(this);
    m_spectrumXRangeModeCombo->addItem("Auto", 0);
    m_spectrumXRangeModeCombo->addItem("Manual", 1);
    layout->addRow("X Range:", m_spectrumXRangeModeCombo);

    m_spectrumManualXMinSpin = new QDoubleSpinBox(this);
    m_spectrumManualXMinSpin->setDecimals(2);
    m_spectrumManualXMinSpin->setRange(-1000000.0, 1000000.0);
    m_spectrumManualXMinSpin->setValue(0.0);

    m_spectrumManualXMaxSpin = new QDoubleSpinBox(this);
    m_spectrumManualXMaxSpin->setDecimals(2);
    m_spectrumManualXMaxSpin->setRange(-1000000.0, 1000000.0);
    m_spectrumManualXMaxSpin->setValue(100.0);

    auto *xManualLayout = new QHBoxLayout();
    xManualLayout->addWidget(m_spectrumManualXMinSpin);
    xManualLayout->addWidget(m_spectrumManualXMaxSpin);
    layout->addRow("X Min / Max:", xManualLayout);

    m_spectrumYRangeModeCombo = new QComboBox(this);
    m_spectrumYRangeModeCombo->addItem("Auto", 0);
    m_spectrumYRangeModeCombo->addItem("Manual", 1);
    layout->addRow("Y Range:", m_spectrumYRangeModeCombo);

    m_spectrumManualYMinSpin = new QDoubleSpinBox(this);
    m_spectrumManualYMinSpin->setDecimals(2);
    m_spectrumManualYMinSpin->setRange(-1000000.0, 1000000.0);
    m_spectrumManualYMinSpin->setValue(0.0);

    m_spectrumManualYMaxSpin = new QDoubleSpinBox(this);
    m_spectrumManualYMaxSpin->setDecimals(2);
    m_spectrumManualYMaxSpin->setRange(-1000000.0, 1000000.0);
    m_spectrumManualYMaxSpin->setValue(100.0);

    auto *yManualLayout = new QHBoxLayout();
    yManualLayout->addWidget(m_spectrumManualYMinSpin);
    yManualLayout->addWidget(m_spectrumManualYMaxSpin);
    layout->addRow("Y Min / Max:", yManualLayout);

    connect(m_spectrumScaleTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onSpectrumScaleTypeChanged);
    connect(m_spectrumXRangeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onSpectrumXRangeModeChanged);
    connect(m_spectrumYRangeModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScaleControlDialog::onSpectrumYRangeModeChanged);

    connect(m_spectrumManualXMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { onSpectrumManualXMinChanged(); });
    connect(m_spectrumManualXMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { onSpectrumManualXMaxChanged(); });
    connect(m_spectrumManualYMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { onSpectrumManualYMinChanged(); });
    connect(m_spectrumManualYMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { onSpectrumManualYMaxChanged(); });

    updateSpectrumManualXEnabled();
    updateSpectrumManualYEnabled();
}
```

- [ ] **Step 3: Add the `updateSpectrumManualXEnabled` / `updateSpectrumManualYEnabled` helpers**

After `createSpectrumGroup()` (currently ends around line 65), add:

```cpp
void ScaleControlDialog::updateSpectrumManualXEnabled()
{
    bool manual = m_spectrumXRangeModeCombo->currentData().toInt() == 1;
    m_spectrumManualXMinSpin->setEnabled(manual);
    m_spectrumManualXMaxSpin->setEnabled(manual);
}

void ScaleControlDialog::updateSpectrumManualYEnabled()
{
    bool manual = m_spectrumYRangeModeCombo->currentData().toInt() == 1;
    m_spectrumManualYMinSpin->setEnabled(manual);
    m_spectrumManualYMaxSpin->setEnabled(manual);
}
```

- [ ] **Step 4: Add setter/getter implementations**

After `setSpectrumScaleType` (currently ends around line 95), add:

```cpp
void ScaleControlDialog::setSpectrumXRangeMode(int mode)
{
    int index = m_spectrumXRangeModeCombo->findData(mode);
    if (index >= 0) {
        m_spectrumXRangeModeCombo->blockSignals(true);
        m_spectrumXRangeModeCombo->setCurrentIndex(index);
        m_spectrumXRangeModeCombo->blockSignals(false);
        updateSpectrumManualXEnabled();
    }
}

void ScaleControlDialog::setSpectrumYRangeMode(int mode)
{
    int index = m_spectrumYRangeModeCombo->findData(mode);
    if (index >= 0) {
        m_spectrumYRangeModeCombo->blockSignals(true);
        m_spectrumYRangeModeCombo->setCurrentIndex(index);
        m_spectrumYRangeModeCombo->blockSignals(false);
        updateSpectrumManualYEnabled();
    }
}

void ScaleControlDialog::setSpectrumManualXRange(double min, double max)
{
    m_spectrumManualXMinSpin->blockSignals(true);
    m_spectrumManualXMaxSpin->blockSignals(true);
    m_spectrumManualXMinSpin->setValue(min);
    m_spectrumManualXMaxSpin->setValue(max);
    m_spectrumManualXMinSpin->blockSignals(false);
    m_spectrumManualXMaxSpin->blockSignals(false);
}

void ScaleControlDialog::setSpectrumManualYRange(double min, double max)
{
    m_spectrumManualYMinSpin->blockSignals(true);
    m_spectrumManualYMaxSpin->blockSignals(true);
    m_spectrumManualYMinSpin->setValue(min);
    m_spectrumManualYMaxSpin->setValue(max);
    m_spectrumManualYMinSpin->blockSignals(false);
    m_spectrumManualYMaxSpin->blockSignals(false);
}
```

After `int spectrumScaleType() const` (currently around line 110), add:

```cpp
int ScaleControlDialog::spectrumXRangeMode() const
{
    return m_spectrumXRangeModeCombo->currentData().toInt();
}

int ScaleControlDialog::spectrumYRangeMode() const
{
    return m_spectrumYRangeModeCombo->currentData().toInt();
}

double ScaleControlDialog::spectrumManualXMin() const
{
    return m_spectrumManualXMinSpin->value();
}

double ScaleControlDialog::spectrumManualXMax() const
{
    return m_spectrumManualXMaxSpin->value();
}

double ScaleControlDialog::spectrumManualYMin() const
{
    return m_spectrumManualYMinSpin->value();
}

double ScaleControlDialog::spectrumManualYMax() const
{
    return m_spectrumManualYMaxSpin->value();
}
```

- [ ] **Step 5: Add the new slot implementations**

After `onSpectrumScaleTypeChanged` (currently around line 128), add:

```cpp
void ScaleControlDialog::onSpectrumXRangeModeChanged(int index)
{
    Q_UNUSED(index)
    updateSpectrumManualXEnabled();
    emit spectrumXRangeModeChanged(spectrumXRangeMode());
    if (spectrumXRangeMode() == 1) {
        emit spectrumManualXRangeChanged(spectrumManualXMin(), spectrumManualXMax());
    }
}

void ScaleControlDialog::onSpectrumYRangeModeChanged(int index)
{
    Q_UNUSED(index)
    updateSpectrumManualYEnabled();
    emit spectrumYRangeModeChanged(spectrumYRangeMode());
    if (spectrumYRangeMode() == 1) {
        emit spectrumManualYRangeChanged(spectrumManualYMin(), spectrumManualYMax());
    }
}

void ScaleControlDialog::onSpectrumManualXMinChanged()
{
    if (spectrumXRangeMode() == 1) {
        emit spectrumManualXRangeChanged(spectrumManualXMin(), spectrumManualXMax());
    }
}

void ScaleControlDialog::onSpectrumManualXMaxChanged()
{
    if (spectrumXRangeMode() == 1) {
        emit spectrumManualXRangeChanged(spectrumManualXMin(), spectrumManualXMax());
    }
}

void ScaleControlDialog::onSpectrumManualYMinChanged()
{
    if (spectrumYRangeMode() == 1) {
        emit spectrumManualYRangeChanged(spectrumManualYMin(), spectrumManualYMax());
    }
}

void ScaleControlDialog::onSpectrumManualYMaxChanged()
{
    if (spectrumYRangeMode() == 1) {
        emit spectrumManualYRangeChanged(spectrumManualYMin(), spectrumManualYMax());
    }
}
```

- [ ] **Step 6: Confirm GUI compiles**

```bash
.\build_preset.bat debug 2>&1 | Select-String -Pattern "error|Error" | Select-Object -First 30
```

Expected: zero errors.

- [ ] **Step 7: Commit the dialog implementation**

```bash
git add src/gui/widgets/dialogs/ScaleControlDialog.cpp
git commit -m "feat(scale-dialog): wire spectrum X/Y range mode controls"
```

---

## Task 6: Wire dialog signals in MainWindow and initialise new state

**Files:**
- Modify: `src/gui/widgets/MainWindow.cpp`

- [ ] **Step 1: Remove the dead include**

Edit `src/gui/widgets/MainWindow.cpp`. Delete line 11 (the `#include "dialogs/CustomRangeDialog.h"` line). Do not touch other includes.

- [ ] **Step 2: Add initial-state setters after `setSpectrumScaleType(0)`**

Find the existing block (around lines 75–78):

```cpp
    m_scaleDialog = new ScaleControlDialog(this);
    m_scaleDialog->setImageScaleType(0);
    m_scaleDialog->setImageColorScaleMode(0);
    m_scaleDialog->setSpectrumScaleType(0);
```

Add two lines after the `setSpectrumScaleType(0)` call:

```cpp
    m_scaleDialog->setSpectrumScaleType(0);
    m_scaleDialog->setSpectrumXRangeMode(0);
    m_scaleDialog->setSpectrumYRangeMode(0);
```

- [ ] **Step 3: Add four signal connections**

Find the existing `connect` block for the spectrum scale type (around lines 100–107):

```cpp
    connect(m_scaleDialog, &ScaleControlDialog::spectrumScaleTypeChanged,
            this, [this](int type) {
                if (m_spectrumViewWidget) {
                    m_spectrumViewWidget->setIntensityScaleType(
                        type == 0 ? SpectrumViewWidget::IntensityScaleType::Auto
                                  : SpectrumViewWidget::IntensityScaleType::Log);
                }
            });
```

Append the four new connections directly after that block (still inside the constructor):

```cpp
    connect(m_scaleDialog, &ScaleControlDialog::spectrumXRangeModeChanged,
            this, [this](int mode) {
                if (m_spectrumViewWidget) {
                    m_spectrumViewWidget->setXAxisRangeMode(
                        mode == 0 ? SpectrumViewWidget::AxisRangeMode::Auto
                                  : SpectrumViewWidget::AxisRangeMode::Manual);
                }
            });

    connect(m_scaleDialog, &ScaleControlDialog::spectrumYRangeModeChanged,
            this, [this](int mode) {
                if (m_spectrumViewWidget) {
                    m_spectrumViewWidget->setYAxisRangeMode(
                        mode == 0 ? SpectrumViewWidget::AxisRangeMode::Auto
                                  : SpectrumViewWidget::AxisRangeMode::Manual);
                }
            });

    connect(m_scaleDialog, &ScaleControlDialog::spectrumManualXRangeChanged,
            this, [this](double min, double max) {
                if (m_spectrumViewWidget) {
                    m_spectrumViewWidget->setManualXRange(min, max);
                }
            });

    connect(m_scaleDialog, &ScaleControlDialog::spectrumManualYRangeChanged,
            this, [this](double min, double max) {
                if (m_spectrumViewWidget) {
                    m_spectrumViewWidget->setManualYRange(min, max);
                }
            });
```

- [ ] **Step 4: Build and verify**

```bash
.\build_preset.bat debug 2>&1 | Select-String -Pattern "error|Error" | Select-Object -First 30
```

Expected: zero errors.

- [ ] **Step 5: Commit**

```bash
git add src/gui/widgets/MainWindow.cpp
git commit -m "feat(main): wire spectrum X/Y range controls and drop dead include"
```

---

## Task 7: Delete CustomRangeDialog

**Files:**
- Delete: `src/gui/widgets/dialogs/CustomRangeDialog.h`
- Delete: `src/gui/widgets/dialogs/CustomRangeDialog.cpp`
- Modify: `src/gui/CMakeLists.txt`

- [ ] **Step 1: Confirm no remaining references**

Run:

```bash
Select-String -Path "D:\10_Projects\2502-Sw-EZSpecCam-shadow" -Pattern "CustomRangeDialog" -SimpleMatch
```

Expected: only the two files we are about to delete. If anything else mentions the class, stop and resolve before continuing.

- [ ] **Step 2: Delete the files**

```bash
Remove-Item -LiteralPath "D:\10_Projects\2502-Sw-EZSpecCam-shadow\src\gui\widgets\dialogs\CustomRangeDialog.h"
Remove-Item -LiteralPath "D:\10_Projects\2502-Sw-EZSpecCam-shadow\src\gui\widgets\dialogs\CustomRangeDialog.cpp"
```

- [ ] **Step 3: Remove the entries from `src/gui/CMakeLists.txt`**

Edit `src/gui/CMakeLists.txt`. Delete line 38:

```
    widgets/dialogs/CustomRangeDialog.h widgets/dialogs/CustomRangeDialog.cpp
```

- [ ] **Step 4: Build to confirm clean removal**

```bash
.\build_preset.bat debug 2>&1 | Select-String -Pattern "error|Error" | Select-Object -First 30
```

Expected: zero errors.

- [ ] **Step 5: Commit**

```bash
git add -u src/gui/widgets/dialogs src/gui/CMakeLists.txt
git commit -m "refactor: remove unused CustomRangeDialog"
```

---

## Task 8: Full verification

**Files:** none (read-only)

- [ ] **Step 1: Run the new test module**

```bash
& "build\msvc-debug\bin\Debug\test_spectrum_axis_range.exe" -v2 -o test_axis_range.txt 2>&1; Get-Content test_axis_range.txt -Tail 80
```

Expected: 9/9 tests pass.

- [ ] **Step 2: Run the existing spectrum view perf test (regression check)**

```bash
& "build\msvc-debug\bin\Debug\test_view_widgets_perf.exe" -v2 -o test_perf.txt 2>&1; Get-Content test_perf.txt -Tail 60
```

Expected: existing tests still pass; the spectrum timings are reported without crashing.

- [ ] **Step 3: Run the full ctest suite**

```bash
ctest --test-dir build/msvc-debug -C Debug --output-on-failure
```

Expected: all tests pass; only the spectrum view perf stats produce console output without failure.

- [ ] **Step 4: Final commit if any stray edits**

```bash
git status --short
```

If anything is dirty, commit it with a descriptive message. Then:

```bash
git log --oneline -10
```

Expected: the head commit is the most recent task above.

---

## Self-Review Notes

- **Spec coverage:**
  - Auto/Manual mode on X and Y — Task 3 implements `applyAxisRange`, Tasks 2/4 expose API.
  - Right-click reset for both axes — Task 3 updates `resetZoom` + `eventFilter` call site.
  - New data preserves zoom — Task 3 keeps `m_userHasZoomed` guard in `applyAxisRange`; covered by test `test_new_data_preserves_zoom`.
  - Manual range settable via dialog — Tasks 4/5/6 wire spin boxes through dialog signals.
  - CustomRangeDialog removed — Task 7.
- **Placeholders:** none.
- **Type consistency:** `AxisRangeMode` is used identically in widget header, widget cpp, dialog header, dialog cpp, MainWindow lambda, and tests. `currentYMin/Max`, `setManualYRange`, `setZoomedForTest`, `resetZoom` all match between header declarations and test usage.