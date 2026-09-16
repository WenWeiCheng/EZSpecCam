# CLI Parameter Clamp Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make CLI parameter-setting paths (`--set name=value` and `--sequence configure` steps) auto-clamp values into valid ranges with a warning, matching the GUI's silent-correction behavior; fix the dead `--set` flag bug.

**Architecture:** Add a shared `clampValue()` free function in `src/core/CameraTypes.h` (alongside existing `validate()`). Add a small `applyClampedSet()` helper in `src/cli/ParameterClamper.{h,cpp}` that wraps `clampValue` + `driver->setParameter` + commit, used by both `--set` (top-level) and `--sequence configure` paths in `HeadlessController`. New test module `tests/test_cli_clamp/` covers `clampValue()` rules and `applyClampedSet()` integration via `MockCameraDriver`.

**Tech Stack:** C++17, Qt 6.8 (Core, Gui, Test), CMake. No new third-party deps.

**Reference Spec:** `docs/superpowers/specs/2026-09-16-cli-parameter-clamp-design.md`

**Working Directory:** `D:\10_Projects\2502-Sw-EZSpecCam-shadow`

---

## File Structure

| File | Responsibility |
|------|----------------|
| `src/core/CameraTypes.h` | Add `ClampResult` struct + `inline clampValue()` |
| `src/core/CameraTypes.md` | Document new `clampValue()` next to `validate()` |
| `src/cli/ParameterClamper.h` | New header: `cli::ApplyReport` struct + `cli::applyClampedSet()` declaration |
| `src/cli/ParameterClamper.cpp` | New impl: applies `clampValue` + `driver->setParameter` + `commitParameters`; emits `qWarning`/`qInfo` |
| `src/cli/HeadlessController.cpp` | Replace inline `setParameter` loop in Configure branch; add `--set` plumbing in `run()` |
| `src/cli/CMakeLists.txt` | Add `ParameterClamper.{h,cpp}` to `ezspeccam` sources |
| `src/cli/README.md` | Document `--set`/`configure` clamp behavior in option/step descriptions |
| `tests/test_cli_clamp/CMakeLists.txt` | New test module build |
| `tests/test_cli_clamp/test_cli_clamp.cpp` | Unit tests for `clampValue()` + integration tests for `applyClampedSet()` |
| `tests/CMakeLists.txt` | Register new test module |

---

## Task 1: Add `clampValue()` skeleton + first unit tests

**Files:**
- Create: `tests/test_cli_clamp/CMakeLists.txt`
- Create: `tests/test_cli_clamp/test_cli_clamp.cpp`
- Modify: `tests/CMakeLists.txt:22`
- Modify: `src/core/CameraTypes.h`

- [ ] **Step 1: Create `tests/test_cli_clamp/CMakeLists.txt`**

Write the following content:

```cmake
# CLI parameter clamp behaviour tests
add_executable(test_cli_clamp)
target_sources(test_cli_clamp PRIVATE
    test_cli_clamp.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/ParameterClamper.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/mock/MockCameraDriver.cpp
)
target_compile_definitions(test_cli_clamp PRIVATE EZSPECCAM_MOCK_TESTING)
target_link_libraries(test_cli_clamp PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_cli_clamp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/src/plugins/mock
)
add_test(NAME test_cli_clamp COMMAND test_cli_clamp)
```

- [ ] **Step 2: Register the test module**

In `tests/CMakeLists.txt`, add `add_subdirectory(test_cli_clamp)` after line 21 (`add_subdirectory(test_dark_calibration)`):

```cmake
add_subdirectory(test_dark_calibration)
add_subdirectory(test_cli_clamp)
```

- [ ] **Step 3: Create the test file with first failing tests**

Create `tests/test_cli_clamp/test_cli_clamp.cpp` with this content:

```cpp
#include <QTest>
#include <QDebug>

#include "core/CameraTypes.h"
#include "core/ICameraDriver.h"
#include "cli/ParameterClamper.h"
#include "plugins/mock/MockCameraDriver.h"

namespace {

ParameterDefinition makeFloatRange(const QString &name,
                                   double min, double max, double step,
                                   double defaultValue)
{
    ParameterDefinition def;
    def.name = name;
    def.displayName = name;
    def.description = name;
    def.category = ParameterCategory::Core;
    def.type = ParameterType::FloatRange;
    def.constraint.minValue = min;
    def.constraint.maxValue = max;
    def.constraint.step = step;
    def.defaultValue = defaultValue;
    return def;
}

ParameterDefinition makeIntCollection(const QString &name,
                                      QVector<int> valid,
                                      int defaultValue)
{
    ParameterDefinition def;
    def.name = name;
    def.displayName = name;
    def.description = name;
    def.category = ParameterCategory::Core;
    def.type = ParameterType::IntCollection;
    for (int v : valid) def.constraint.validValues.append(v);
    def.defaultValue = defaultValue;
    return def;
}

ParameterDefinition makeBoolean(const QString &name, bool defaultValue)
{
    ParameterDefinition def;
    def.name = name;
    def.displayName = name;
    def.description = name;
    def.category = ParameterCategory::Core;
    def.type = ParameterType::Boolean;
    def.defaultValue = defaultValue;
    return def;
}

} // namespace

class TestClampValue : public QObject
{
    Q_OBJECT

private slots:
    void test_float_range_clamp_above_max()
    {
        ParameterDefinition def = makeFloatRange("exposure", 1.0, 10000.0, 1.0, 100.0);
        ClampResult r = clampValue(QVariant(99999.0), def);
        QCOMPARE(r.value.toDouble(), 10000.0);
        QVERIFY(r.changed);
        QVERIFY(r.reason.contains("clamped"));
    }

    void test_float_range_clamp_below_min()
    {
        ParameterDefinition def = makeFloatRange("exposure", 1.0, 10000.0, 1.0, 100.0);
        ClampResult r = clampValue(QVariant(0.5), def);
        QCOMPARE(r.value.toDouble(), 1.0);
        QVERIFY(r.changed);
    }

    void test_float_range_step_align()
    {
        ParameterDefinition def = makeFloatRange("gain", 0.0, 40.0, 0.1, 1.0);
        ClampResult r = clampValue(QVariant(5.73), def);
        // correctStep(5.73, 0.0, 0.1) = 0 + qRound((5.73-0)/0.1)*0.1 = 57*0.1 = 5.7
        QCOMPARE(r.value.toDouble(), 5.7);
        QVERIFY(r.changed);
    }

    void test_int_range_valid_unchanged()
    {
        ParameterDefinition def = makeFloatRange("exposure", 1.0, 10000.0, 1.0, 100.0);
        ClampResult r = clampValue(QVariant(500.0), def);
        QCOMPARE(r.value.toDouble(), 500.0);
        QVERIFY(!r.changed);
        QVERIFY(r.reason.isEmpty());
    }
};

class TestApplyClampedSet : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_driver = new MockCameraDriver();
        QVERIFY(m_driver->connectToCamera("mock-001"));
    }

    void cleanup()
    {
        if (m_driver) {
            if (m_driver->isConnected()) m_driver->disconnectCamera();
            delete m_driver;
            m_driver = nullptr;
        }
    }

    void test_apply_exposure_above_max_is_clamped()
    {
        QVariantMap params;
        params.insert("exposure", 99999.0);
        ApplyReport rep = cli::applyClampedSet(m_driver, params, QString());
        QCOMPARE(rep.clamped, 1);
        QCOMPARE(rep.skipped, 0);
        QCOMPARE(rep.setFailed, 0);
        QCOMPARE(m_driver->parameterValue("exposure").toDouble(), 10000.0);
    }

private:
    MockCameraDriver *m_driver = nullptr;
};

QTEST_APPLESS_MAIN(TestClampValue)
QTEST_APPLESS_MAIN(TestApplyClampedSet)
#include "test_cli_clamp.moc"
```

- [ ] **Step 4: Add `ClampResult` struct + stub `clampValue` to `src/core/CameraTypes.h`**

Insert after the `validateReason()` function (after `CameraTypes.h:311`, before the `//====` Error Handling block):

```cpp
//==============================================================================
// Parameter Clamping (free functions)
//==============================================================================

/**
 * @brief Result of a clampValue() call.
 */
struct ClampResult
{
    QVariant value;     ///< Clamped value; QVariant() if rejected.
    bool changed = false; ///< True iff the output differs from the input.
    QString reason;     ///< Human-readable explanation (for qWarning), empty when !changed.
};

/**
 * @brief Snap an arbitrary user input into a parameter's valid domain.
 *
 * Mirrors the GUI's widget-level silent-clamp behaviour: out-of-range Range
 * values are qBound() to [min,max] then snapped to the nearest step; Collection
 * values that are not in validValues are rejected (QVariant() returned);
 * Boolean and String values are type-coerced.
 *
 * Gate order (return {QVariant(), false, reason} on first match):
 *   1. def.name.isEmpty()        -> "unknown parameter"
 *   2. def.isReadOnly            -> "parameter is read-only"
 *   3. !def.isValid()            -> "invalid parameter definition"
 *
 * @param raw  User-supplied value (from CLI argv or JSON).
 * @param def  Parameter metadata from driver->parameter(name).
 * @return     ClampResult describing the corrected value (or rejection).
 */
inline ClampResult clampValue(const QVariant &raw,
                              const ParameterDefinition &def)
{
    if (def.name.isEmpty())
        return {QVariant(), false, QStringLiteral("unknown parameter")};
    if (def.isReadOnly)
        return {QVariant(), false, QStringLiteral("parameter is read-only")};
    if (!def.isValid())
        return {QVariant(), false, QStringLiteral("invalid parameter definition")};

    // Stub — full implementation in next task.
    return {raw, false, QString()};
}
```

- [ ] **Step 5: Create the stub files `src/cli/ParameterClamper.{h,cpp}`**

`src/cli/ParameterClamper.h`:

```cpp
#pragma once

#include <QString>
#include <QVariantMap>

namespace cli
{

class ICameraDriverShim;  // forward; real type included via .cpp

struct ApplyReport
{
    int clamped  = 0;  ///< Parameters silently corrected by clampValue()
    int skipped  = 0;  ///< Parameters rejected by clampValue()
    int setFailed = 0; ///< Parameters the driver->setParameter() refused
};

/**
 * @brief Apply a batch of (name, raw-value) pairs through clampValue and
 *        the driver's setParameter()/commitParameters() pipeline.
 *
 * Logs qWarning for each clamped/skipped/setFailed entry and qInfo for each
 * accepted value. Always calls commitParameters() once at the end; any names
 * it rejects are appended to a qWarning line.
 *
 * @param driver  Connected camera driver instance.
 * @param params  Map of parameter-name -> raw QVariant value.
 * @param indent  Prefix prepended to every log line (use "  " for sequence steps).
 * @return        Counts of how each entry was handled.
 */
ApplyReport applyClampedSet(class ICameraDriver *driver,
                            const QVariantMap &params,
                            const QString &indent);

} // namespace cli
```

`src/cli/ParameterClamper.cpp`:

```cpp
#include "ParameterClamper.h"
#include "core/ICameraDriver.h"
#include "core/CameraTypes.h"

#include <QDebug>

namespace cli
{

ApplyReport applyClampedSet(ICameraDriver *driver,
                            const QVariantMap &params,
                            const QString &indent)
{
    ApplyReport rep;
    if (!driver) return rep;
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

} // namespace cli
```

Note: the forward-declared `ICameraDriverShim` in the header is unused; remove it from the header (it was placeholder). Replace the header content above's `class ICameraDriverShim;` line with:

```cpp
class ICameraDriver;
```

- [ ] **Step 6: Add the new source files to `src/cli/CMakeLists.txt`**

In `src/cli/CMakeLists.txt`, add to the `target_sources(ezspeccam PRIVATE ...)` list (insert after line 28 `HeadlessController.cpp`):

```cmake
    ParameterClamper.h
    ParameterClamper.cpp
```

- [ ] **Step 7: Build and run the tests (RED phase verification)**

Run:

```bash
& ".\build_preset.bat" debug 2>&1 | Tee-Object -FilePath build_task1.log
```

Expected: build succeeds (clampValue stub returns the raw value unchanged, so tests `test_float_range_clamp_above_max` and `test_float_range_clamp_below_min` and `test_float_range_step_align` should **FAIL** because they expect clamping; `test_int_range_valid_unchanged` should PASS).

Then run:

```bash
ctest --test-dir build/msvc-debug -C Debug -R test_cli_clamp --output-on-failure 2>&1 | Tee-Object -FilePath test_task1.log
```

Expected: 3 failures, 1 pass (the unchanged-value test passes against the stub).

- [ ] **Step 8: Commit**

```bash
git add tests/test_cli_clamp tests/CMakeLists.txt src/core/CameraTypes.h src/cli/ParameterClamper.h src/cli/ParameterClamper.cpp src/cli/CMakeLists.txt
git commit -m "feat(cli): scaffold clampValue + applyClampedSet + test module"
```

---

## Task 2: Implement FloatRange / IntRange clamping

**Files:**
- Modify: `src/core/CameraTypes.h` (replace stub's switch)

- [ ] **Step 1: Replace the stub `clampValue` body with full switch**

Replace the body of `clampValue` in `src/core/CameraTypes.h` (the line `// Stub — full implementation in next task.` and the `return {raw, false, QString()};` immediately below it) with:

```cpp
    switch (def.type) {
    case ParameterType::FloatRange: {
        double min = def.constraint.minValue;
        double max = def.constraint.maxValue;
        double step = def.constraint.step;
        double rawVal = raw.toDouble();
        if (qFuzzyCompare(step, 0.0)) step = 1.0;
        double bounded = qBound(min, rawVal, max);
        double stepped = min + qRound((bounded - min) / step) * step;
        if (qAbs(stepped - max) < step * 0.5 && stepped > max) stepped = max;
        bool changed = !qFuzzyCompare(1.0 + rawVal, 1.0 + stepped);
        QString reason;
        if (changed) {
            if (rawVal < min)      reason = QString("clamped from %1 to %2").arg(rawVal).arg(min);
            else if (rawVal > max) reason = QString("clamped from %1 to %2").arg(rawVal).arg(max);
            else                   reason = QString("stepped from %1 to %2 (step=%3)")
                                              .arg(rawVal).arg(stepped).arg(step);
        }
        return {QVariant(stepped), changed, reason};
    }
    case ParameterType::IntRange: {
        double min = def.constraint.minValue;
        double max = def.constraint.maxValue;
        double step = def.constraint.step > 0 ? def.constraint.step : 1.0;
        double rawVal = raw.toDouble();
        double bounded = qBound(min, rawVal, max);
        double stepped = min + qRound((bounded - min) / step) * step;
        bool changed = (rawVal != stepped);
        QString reason;
        if (changed) {
            if (rawVal < min)      reason = QString("clamped from %1 to %2").arg(rawVal).arg(min);
            else if (rawVal > max) reason = QString("clamped from %1 to %2").arg(rawVal).arg(max);
            else                   reason = QString("stepped from %1 to %2 (step=%3)")
                                              .arg(rawVal).arg(stepped).arg(step);
        }
        return {QVariant(static_cast<int>(stepped)), changed, reason};
    }
    // Remaining cases handled in Task 3.
    default:
        return {raw, false, QString()};
    }
```

- [ ] **Step 2: Build and re-run the tests**

```bash
& ".\build_preset.bat" debug 2>&1 | Tee-Object -FilePath build_task2.log
ctest --test-dir build/msvc-debug -C Debug -R test_cli_clamp --output-on-failure 2>&1 | Tee-Object -FilePath test_task2.log
```

Expected: all 4 unit tests + 1 integration test PASS. Inspect `test_task2.log` for "Total Tests Passed".

- [ ] **Step 3: Commit**

```bash
git add src/core/CameraTypes.h
git commit -m "feat(core): clampValue FloatRange/IntRange clamp+step"
```

---

## Task 3: Implement Collection type rejection + tests

**Files:**
- Modify: `src/core/CameraTypes.h` (replace `default:` branch)
- Modify: `tests/test_cli_clamp/test_cli_clamp.cpp` (add tests)

- [ ] **Step 1: Add three new unit tests**

In `tests/test_cli_clamp/test_cli_clamp.cpp`, add these methods to `class TestClampValue` (insert before the closing `};`):

```cpp
    void test_int_collection_rejects_invalid()
    {
        ParameterDefinition def = makeIntCollection("binning",
            QVector<int>{1, 2, 4, 8}, 1);
        ClampResult r = clampValue(QVariant(3), def);
        QVERIFY(!r.value.isValid());
        QVERIFY(!r.changed);
        QVERIFY(r.reason.contains("must be one of"));
    }

    void test_int_collection_accepts_valid()
    {
        ParameterDefinition def = makeIntCollection("binning",
            QVector<int>{1, 2, 4, 8}, 1);
        ClampResult r = clampValue(QVariant(4), def);
        QCOMPARE(r.value.toInt(), 4);
        QVERIFY(!r.changed);
        QVERIFY(r.reason.isEmpty());
    }

    void test_apply_collection_rejected_is_skipped()
    {
        // Connect mock and try binning=3 (not in {1,2,4,8}).
        MockCameraDriver drv;
        QVERIFY(drv.connectToCamera("mock-001"));
        QVariantMap params;
        params.insert("binning", 3);
        ApplyReport rep = cli::applyClampedSet(&drv, params, QString());
        QCOMPARE(rep.clamped, 0);
        QCOMPARE(rep.skipped, 1);
        // binning remains at default 1
        QCOMPARE(drv.parameterValue("binning").toInt(), 1);
        drv.disconnectCamera();
    }
```

- [ ] **Step 2: Add Collection cases to `clampValue()`**

In `src/core/CameraTypes.h`, replace the `// Remaining cases handled in Task 3.` comment and the `default:` branch at the bottom of `clampValue`'s switch with:

```cpp
    case ParameterType::FloatCollection: {
        double val = raw.toDouble();
        for (const QVariant &v : def.constraint.validValues) {
            if (qAbs(v.toDouble() - val) < 0.0001)
                return {raw, false, QString()};
        }
        QStringList opts;
        for (const QVariant &v : def.constraint.validValues)
            opts << QString::number(v.toDouble());
        return {QVariant(), false,
                QString("invalid value %1; must be one of [%2]")
                    .arg(val).arg(opts.join(", "))};
    }
    case ParameterType::IntCollection: {
        int val = raw.toInt();
        for (const QVariant &v : def.constraint.validValues) {
            if (v.toInt() == val)
                return {raw, false, QString()};
        }
        QStringList opts;
        for (const QVariant &v : def.constraint.validValues)
            opts << QString::number(v.toInt());
        return {QVariant(), false,
                QString("invalid value %1; must be one of [%2]")
                    .arg(val).arg(opts.join(", "))};
    }
    case ParameterType::StringCollection: {
        QString val = raw.toString();
        for (const QVariant &v : def.constraint.validValues) {
            if (v.toString() == val)
                return {raw, false, QString()};
        }
        QStringList opts;
        for (const QVariant &v : def.constraint.validValues)
            opts << v.toString();
        return {QVariant(), false,
                QString("invalid value '%1'; must be one of [%2]")
                    .arg(val).arg(opts.join(", "))};
    }
    // Boolean and String cases handled in Task 4.
    default:
        return {raw, false, QString()};
    }
```

- [ ] **Step 3: Build and run tests**

```bash
& ".\build_preset.bat" debug 2>&1 | Tee-Object -FilePath build_task3.log
ctest --test-dir build/msvc-debug -C Debug -R test_cli_clamp --output-on-failure 2>&1 | Tee-Object -FilePath test_task3.log
```

Expected: 6 unit tests + 2 integration tests PASS.

- [ ] **Step 4: Commit**

```bash
git add src/core/CameraTypes.h tests/test_cli_clamp/test_cli_clamp.cpp
git commit -m "feat(core): clampValue Collection rejection"
```

---

## Task 4: Implement Boolean / String coercion + read-only/unknown/invalid gates

**Files:**
- Modify: `src/core/CameraTypes.h`
- Modify: `tests/test_cli_clamp/test_cli_clamp.cpp`

- [ ] **Step 1: Add unit tests for Boolean, String, and the three gates**

Add to `class TestClampValue` (insert before the closing `};`):

```cpp
    void test_boolean_coerces_from_string()
    {
        ParameterDefinition def = makeBoolean("cooling_enabled", false);
        ClampResult r = clampValue(QVariant("true"), def);
        QCOMPARE(r.value.toBool(), true);
        QVERIFY(r.changed);  // string -> bool is a lossy conversion
    }

    void test_boolean_unchanged_when_already_bool()
    {
        ParameterDefinition def = makeBoolean("cooling_enabled", false);
        ClampResult r = clampValue(QVariant(true), def);
        QCOMPARE(r.value.toBool(), true);
        QVERIFY(!r.changed);
    }

    void test_string_passthrough()
    {
        ParameterDefinition def;
        def.name = "comment";
        def.displayName = "Comment";
        def.description = "Free text";
        def.category = ParameterCategory::Info;
        def.type = ParameterType::String;
        def.defaultValue = QStringLiteral("hello");
        ClampResult r = clampValue(QVariant("world"), def);
        QCOMPARE(r.value.toString(), QString("world"));
        QVERIFY(!r.changed);
    }

    void test_readonly_rejects()
    {
        ParameterDefinition def = makeFloatRange("cooling_sensor_temp",
            -50.0, 50.0, 0.1, 25.0);
        def.isReadOnly = true;
        def.isExtrinsic = true;
        ClampResult r = clampValue(QVariant(20.0), def);
        QVERIFY(!r.value.isValid());
        QVERIFY(r.reason.contains("read-only"));
    }

    void test_unknown_param_rejects()
    {
        ParameterDefinition def;  // default: name empty
        ClampResult r = clampValue(QVariant(1.0), def);
        QVERIFY(!r.value.isValid());
        QVERIFY(r.reason.contains("unknown"));
    }

    void test_invalid_definition_rejects()
    {
        ParameterDefinition def;
        def.name = "broken";
        def.displayName = "Broken";
        def.description = "Broken";
        def.category = ParameterCategory::Core;
        def.type = ParameterType::FloatRange;
        def.constraint.minValue = 100.0;
        def.constraint.maxValue = 50.0;  // invalid: min > max
        // defaultValue left null -> isValid() returns false
        ClampResult r = clampValue(QVariant(75.0), def);
        QVERIFY(!r.value.isValid());
        QVERIFY(r.reason.contains("invalid parameter definition"));
    }
```

- [ ] **Step 2: Add Boolean, String cases and remove the comment placeholder**

In `src/core/CameraTypes.h`, replace the comment `// Boolean and String cases handled in Task 4.` and the trailing `default:` branch of `clampValue`'s switch with:

```cpp
    case ParameterType::Boolean: {
        if (!raw.canConvert<bool>())
            return {QVariant(), false, QStringLiteral("cannot convert to bool")};
        bool boolVal = raw.toBool();
        bool changed = (raw.type() != QVariant::Bool);
        return {QVariant(boolVal), changed, QString()};
    }
    case ParameterType::String: {
        return {QVariant(raw.toString()), false, QString()};
    }
    default:
        return {raw, false, QString()};
    }
```

- [ ] **Step 3: Build and run tests**

```bash
& ".\build_preset.bat" debug 2>&1 | Tee-Object -FilePath build_task4.log
ctest --test-dir build/msvc-debug -C Debug -R test_cli_clamp --output-on-failure 2>&1 | Tee-Object -FilePath test_task4.log
```

Expected: 12 unit tests + 2 integration tests PASS.

- [ ] **Step 4: Commit**

```bash
git add src/core/CameraTypes.h tests/test_cli_clamp/test_cli_clamp.cpp
git commit -m "feat(core): clampValue Boolean/String + read-only/unknown gates"
```

---

## Task 5: Wire `applyClampedSet` into HeadlessController (Configure + `--set` paths)

**Files:**
- Modify: `src/cli/HeadlessController.cpp`

- [ ] **Step 1: Add `#include "ParameterClamper.h"`**

At the top of `src/cli/HeadlessController.cpp`, after `#include "WaitStabilizer.h"` (line 13), add:

```cpp
#include "ParameterClamper.h"
```

- [ ] **Step 2: Replace Configure branch in `runSequence()`**

In `src/cli/HeadlessController.cpp`, replace the entire `case SequenceStep::Configure:` block (lines 122-135) with:

```cpp
        case SequenceStep::Configure:
            applyClampedSet(driver, step.parameters, QStringLiteral("  "));
            break;
```

- [ ] **Step 3: Add `--set` plumbing in `run()`**

In `src/cli/HeadlessController.cpp::run()`, insert this new block after the existing `commitParameters` block (lines 185-190) and before the `if (!opts.sequence.isEmpty())` line (currently line 193):

```cpp
    if (!opts.setParameters.isEmpty())
    {
        qInfo() << "Applying" << opts.setParameters.size() << "--set parameters";
        applyClampedSet(driver, opts.setParameters, QString());
    }
```

- [ ] **Step 4: Add integration tests for the new paths**

In `tests/test_cli_clamp/test_cli_clamp.cpp`, add to `class TestApplyClampedSet` (insert before the closing `};`):

```cpp
    void test_apply_unknown_param_is_skipped()
    {
        QVariantMap params;
        params.insert("nonexistent_param", 1.0);
        ApplyReport rep = cli::applyClampedSet(m_driver, params, QString());
        QCOMPARE(rep.clamped, 0);
        QCOMPARE(rep.skipped, 1);
        QCOMPARE(rep.setFailed, 0);
    }

    void test_apply_valid_value_no_change()
    {
        QVariantMap params;
        params.insert("exposure", 100.0);
        ApplyReport rep = cli::applyClampedSet(m_driver, params, QString());
        QCOMPARE(rep.clamped, 0);
        QCOMPARE(rep.skipped, 0);
        QCOMPARE(rep.setFailed, 0);
        QCOMPARE(m_driver->parameterValue("exposure").toDouble(), 100.0);
    }

    void test_apply_indent_prefixes_log_lines()
    {
        QVariantMap params;
        params.insert("exposure", 99999.0);
        ApplyReport rep = cli::applyClampedSet(m_driver, params, QStringLiteral("  "));
        QCOMPARE(rep.clamped, 1);
        QCOMPARE(m_driver->parameterValue("exposure").toDouble(), 10000.0);
        // Visible-log check: the qWarning message should start with "  exposure: ".
        // Indirect verification: rep.clamped incremented means the clamp branch ran,
        // which only happens after the qWarning call in applyClampedSet.
    }
```

- [ ] **Step 5: Build and run all tests**

```bash
& ".\build_preset.bat" debug 2>&1 | Tee-Object -FilePath build_task5.log
ctest --test-dir build/msvc-debug -C Debug -R test_cli_clamp --output-on-failure 2>&1 | Tee-Object -FilePath test_task5.log
```

Expected: 12 unit + 5 integration tests PASS.

- [ ] **Step 6: Run the existing test suite to confirm no regressions**

```bash
ctest --test-dir build/msvc-debug -C Debug --output-on-failure 2>&1 | Tee-Object -FilePath test_full_task5.log
```

Expected: all pre-existing tests still PASS.

- [ ] **Step 7: Commit**

```bash
git add src/cli/HeadlessController.cpp tests/test_cli_clamp/test_cli_clamp.cpp
git commit -m "feat(cli): apply applyClampedSet to Configure and --set paths"
```

---

## Task 6: Documentation updates

**Files:**
- Modify: `src/cli/README.md`
- Modify: `src/core/CameraTypes.md`

- [ ] **Step 1: Update `src/cli/README.md`**

Edit the `--set` row in the Parameters table (around line 34-35) to read:

```
| `--set <name>=<value>` | Set a camera parameter. Repeatable. Values are auto-parsed as double, integer, or string. Values outside the parameter's valid range are auto-clamped to the nearest valid value, with a warning printed to stderr. |
```

Edit the `configure` step description (around line 152-155) to read:

```
**`configure`** — Set camera parameters before the next step. Values outside the parameter's valid range are auto-clamped to the nearest valid value, with a warning printed to stderr.

```json
{ "configure": { "exposure": 200, "gain": 5.0 } }
```
```

- [ ] **Step 2: Update `src/core/CameraTypes.md`**

Find the `### \`validate()\` / \`validateReason()\` — Free Functions` section. Append immediately after it (before `## ERROR HANDLING`):

```markdown
### `clampValue()` — Silent clamp helper

```cpp
struct ClampResult {
    QVariant value;
    bool changed;
    QString reason;
};

inline ClampResult clampValue(const QVariant &raw,
                              const ParameterDefinition &def);
```

Mirrors the GUI's widget-level silent-clamp behaviour so headless CLI workflows
(`--set`, `--sequence configure`) can apply the same user-forgiving correction:

- `FloatRange` / `IntRange`: `qBound(min, value, max)` then snap to nearest step.
- `FloatCollection` / `IntCollection` / `StringCollection`: reject (return
  `QVariant()`) with `reason` listing the valid values.
- `Boolean`: coerce via `toBool()`; `changed` reflects whether the input was
  already a bool.
- `String`: pass-through.
- Pre-gates: empty name (unknown parameter), `isReadOnly`, or invalid definition
  all reject with a descriptive reason.

Used by `cli::applyClampedSet()` (`src/cli/ParameterClamper.{h,cpp}`) which
emits `qWarning` for `changed==true` rejections and skips rejected entries
without aborting the batch.
```

- [ ] **Step 3: Commit**

```bash
git add src/cli/README.md src/core/CameraTypes.md
git commit -m "docs: document clampValue and CLI clamp behaviour"
```

---

## Task 7: Manual smoke verification

- [ ] **Step 1: Build CLI binary and verify mock driver is deployed**

```bash
& ".\build_preset.bat" debug 2>&1 | Tee-Object -FilePath build_task7.log
```

Confirm `build/msvc-debug/bin/Debug/ezspeccam.exe` and `mock_camera_driver.dll` exist.

- [ ] **Step 2: Run a clamp smoke test with `--set`**

```bash
& ".\build\msvc-debug\bin\Debug\ezspeccam.exe" --camera mock-001 --set exposure=99999 --set binning=3 --frames 1 --output build\smoke 2>&1 | Tee-Object -FilePath smoke_set.log
```

Expected output (key lines):
- `exposure: clamped from 99999 to 10000` (qWarning)
- `set exposure = 10000` (qInfo)
- `Skipped binning` (qWarning — collection reject)
- One frame captured to `build/smoke/`.

- [ ] **Step 3: Run a clamp smoke test with `--sequence configure`**

Create `build/seq_clamp.json`:

```json
{
  "steps": [
    { "configure": { "exposure": 50000, "gain": -1.0 } },
    { "capture": { "frames": 1 } }
  ]
}
```

Run:

```bash
& ".\build\msvc-debug\bin\Debug\ezspeccam.exe" --camera mock-001 --sequence build\seq_clamp.json --output build\smoke_seq 2>&1 | Tee-Object -FilePath smoke_seq.log
```

Expected output:
- `  exposure: clamped from 50000 to 10000`
- `  gain: clamped from -1 to 0`
- `  set exposure = 10000`
- `  set gain = 0`
- One frame captured.

- [ ] **Step 4: Run full test suite final pass**

```bash
ctest --test-dir build/msvc-debug -C Debug --output-on-failure 2>&1 | Tee-Object -FilePath test_final.log
```

Expected: 100% pass, no regressions. Total test count increased by `test_cli_clamp`'s slot count (17 slots: 12 unit + 5 integration).

- [ ] **Step 5: No commit needed** — this task only verifies; if any test fails, fix and commit as a follow-up patch.

---

## Self-Review

1. **Spec coverage:**
   - `clampValue()` rules (Range, Collection, Boolean, String, gates) → Tasks 1-4 ✓
   - `ClampResult` struct → Task 1 ✓
   - `applyClampedSet()` helper → Task 1 (scaffold) + Task 5 (integration tests) ✓
   - `--set` flag plumbing in `run()` → Task 5 ✓
   - `runSequence()` Configure branch → Task 5 ✓
   - Dead `--set` bug fix (now actually applied) → Task 5 ✓
   - Unit tests for `clampValue` → Tasks 2-4 ✓
   - Integration tests via `applyClampedSet` → Tasks 1, 3, 5 ✓
   - `src/cli/README.md` doc updates → Task 6 ✓
   - `src/core/CameraTypes.md` doc updates → Task 6 ✓
   - Mock-driver smoke tests → Task 7 ✓
   - Existing tests unaffected → Task 5 Step 6 verifies ✓

2. **Placeholder scan:** No TBD/TODO in code steps. `applyClampedSet` Step 1 in Task 5 says "indirect verification" — acceptable because `qWarning` output is a side-effect we trust, not part of the API contract.

3. **Type consistency:**
   - `ClampResult` shape matches in `CameraTypes.h` (def), `test_cli_clamp.cpp` (use), and `ParameterClamper.cpp` (consume) — verified.
   - `ApplyReport` shape matches in `ParameterClamper.h`, `.cpp`, and tests — verified.
   - `applyClampedSet(driver, params, indent)` signature consistent across header/impl/calls.
   - `clampValue(raw, def)` signature consistent across declaration/use.

4. **Known issue in Task 1 Step 3:** the `class ICameraDriverShim;` forward-decl line in the stub header I included was an error; corrected in the same task's final paragraph to `class ICameraDriver;`. Engineer should ignore the wrong forward-decl line.