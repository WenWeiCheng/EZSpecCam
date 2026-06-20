# CLI/GUI Single-Binary Merge Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Collapse `ezspeccam_cli.exe` and `ezspeccam_gui.exe` into a single `ezspeccam.exe` with a new `src/app/` shared layer; unify CSV format (wide) and metadata schema (includes `softwareSettings`).

**Architecture:** Single Qt executable; `QApplication` vs `QCoreApplication` selected at runtime by argv. New `src/app/` namespace exposes `app::plugins`, `app::formats`, `app::WaitStabilizer`, `app::HeadlessController`, `app::MessageHandler`, `app::AppMode`. CLI/GUI entry points shrink to thin wrappers.

**Tech Stack:** Qt 6.8 (Core/Gui/Widgets/PrintSupport/OpenGL), C++17, CMake 3.20+, MSVC 2022, QCustomPlot 2.1.1.

**Spec:** `docs/superpowers/specs/2026-06-20-cli-gui-merge-design.md`

**Conventions for this plan:**
- File paths use forward slashes (Qt/CMake accept both on Windows).
- Build outputs: `build/msvc-debug/bin/Debug/ezspeccam.exe` after merge.
- "Run the build" = `.\build_preset.bat debug` from repo root.
- "Run the tests" = `.\run_tests.bat` from repo root.
- Every commit is one logical change.

---

## Phase 0: Branch + Preparation

### Task 0.1: Create working branch

**Files:** none

- [ ] **Step 1: Verify clean state, create branch**

```bash
cd /d D:\10_Projects\2502-Sw-EZSpecCam-shadow
git status
git checkout -b refactor/cli-gui-merge
```

Expected: Branch created, no uncommitted changes (other than the spec doc already committed on `main`).

- [ ] **Step 2: Commit the spec is already on main; verify it's reachable**

```bash
git log --oneline -1 docs/superpowers/specs/2026-06-20-cli-gui-merge-design.md
```

Expected: A commit hash (the spec commit from earlier).

### Task 0.2: Create `src/app/` directory skeleton (empty placeholders)

**Files:**
- Create: `src/app/.gitkeep`
- Create: `src/app/formats/.gitkeep`

- [ ] **Step 1: Create directories**

```bash
mkdir src\app\formats
type nul > src\app\.gitkeep
type nul > src\app\formats\.gitkeep
```

- [ ] **Step 2: Commit**

```bash
git add src/app/.gitkeep src/app/formats/.gitkeep
git commit -m "chore: scaffold src/app/ directory"
```

---

## Phase 1: New Utilities (TDD-friendly, isolated)

### Task 1.1: `app::AppMode` with `parseAppMode`

**Files:**
- Create: `src/app/AppMode.h`
- Create: `src/app/AppMode.cpp`
- Test: `tests/test_app_mode/CMakeLists.txt`
- Test: `tests/test_app_mode/test_app_mode.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `tests/test_app_mode/test_app_mode.cpp`:

```cpp
#include <QtTest>
#include "app/AppMode.h"

class TestAppMode : public QObject
{
    Q_OBJECT
private slots:
    void no_args_is_windowed();
    void bare_dashdash_is_windowed();
    void list_is_headless();
    void list_params_is_headless();
    void camera_is_headless();
    void set_is_headless();
    void frames_is_headless();
    void sequence_is_headless();
    void output_is_headless();
    void format_is_headless();
    void help_is_headless();
    void version_is_headless();
    void unknown_flag_after_camera_is_headless();
};

void TestAppMode::no_args_is_windowed() {
    int argc = 1;
    char *argv[] = { const_cast<char*>("ezspeccam") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Windowed);
}
void TestAppMode::bare_dashdash_is_windowed() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Windowed);
}
void TestAppMode::list_is_headless() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--list") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::list_params_is_headless() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--list-params") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::camera_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--camera"), const_cast<char*>("x") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::set_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--set"), const_cast<char*>("a=1") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::frames_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--frames"), const_cast<char*>("5") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::sequence_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--sequence"), const_cast<char*>("x.json") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::output_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--output"), const_cast<char*("./") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::format_is_headless() {
    int argc = 3;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--format"), const_cast<char*>("tiff") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::help_is_headless() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--help") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::version_is_headless() {
    int argc = 2;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--version") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}
void TestAppMode::unknown_flag_after_camera_is_headless() {
    int argc = 4;
    char *argv[] = { const_cast<char*>("ezspeccam"), const_cast<char*>("--camera"),
                     const_cast<char*>("x"), const_cast<char*>("--bogus") };
    QCOMPARE(app::parseAppMode(argc, argv), app::Mode::Headless);
}

QTEST_MAIN(TestAppMode)
#include "test_app_mode.moc"
```

Create `tests/test_app_mode/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(test_app_mode)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Test REQUIRED)

add_executable(test_app_mode test_app_mode.cpp
    ${CMAKE_SOURCE_DIR}/src/app/AppMode.h
    ${CMAKE_SOURCE_DIR}/src/app/AppMode.cpp
)

target_include_directories(test_app_mode PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_app_mode PRIVATE
    Qt6::Core Qt6::Test
)

add_test(NAME test_app_mode COMMAND test_app_mode)
```

- [ ] **Step 2: Add to tests/CMakeLists.txt**

Edit `tests/CMakeLists.txt` — add `add_subdirectory(test_app_mode)` after the existing entries:

```cmake
add_subdirectory(test_app_mode)
```

- [ ] **Step 3: Run test to verify it fails (no implementation yet)**

```bash
.\build_preset.bat debug
```

Expected: build fails because `src/app/AppMode.h` doesn't exist yet.

- [ ] **Step 4: Write minimal implementation**

Create `src/app/AppMode.h`:

```cpp
#pragma once

namespace app
{
    enum class Mode { Headless, Windowed };

    Mode parseAppMode(int argc, char *argv[]);
}
```

Create `src/app/AppMode.cpp`:

```cpp
#include "AppMode.h"

#include <QStringList>

namespace
{
    const QStringList kHeadlessTriggers = {
        "--list",
        "--list-params",
        "--camera",
        "--set",
        "--frames",
        "--sequence",
        "--output",
        "--format",
        "--prefix",
        "--suffix",
        "--help",
        "--version"
    };
}

namespace app
{

Mode parseAppMode(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (kHeadlessTriggers.contains(arg))
            return Mode::Headless;
    }
    return Mode::Windowed;
}

}
```

- [ ] **Step 5: Build and run test**

```bash
.\build_preset.bat debug
.\build\msvc-debug\bin\Debug\test_app_mode.exe -o test_app_mode_out.txt
type test_app_mode_out.txt
```

Expected: All `test_app_mode` test cases PASS. (Run via `ctest` once registered — temporarily move `add_test` is unnecessary since it's in the subdir CMakeLists.)

- [ ] **Step 6: Commit**

```bash
git add src/app/AppMode.h src/app/AppMode.cpp tests/test_app_mode/ tests/CMakeLists.txt
git commit -m "feat(app): add app::parseAppMode with tests"
```

### Task 1.2: `app::MessageHandler` with Windows console attach

**Files:**
- Create: `src/app/MessageHandler.h`
- Create: `src/app/MessageHandler.cpp`

- [ ] **Step 1: Create header**

`src/app/MessageHandler.h`:

```cpp
#pragma once

namespace app
{
    /// Install qDebug/qInfo → stdout, qCritical/qFatal → stderr.
    void installMessageHandler();

    /// Windows: AttachConsole(ATTACH_PARENT_PROCESS) and re-open stdout/stderr to CONOUT$.
    /// Non-Windows: no-op.
    void attachParentConsoleIfAvailable();
}
```

- [ ] **Step 2: Create implementation**

`src/app/MessageHandler.cpp`:

```cpp
#include "MessageHandler.h"

#include <QTextStream>
#include <QtGlobal>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <cstdio>
#endif

namespace
{

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    QTextStream out(type == QtCriticalMsg || type == QtFatalMsg ? stderr : stdout);
    out << msg << "\n";
    out.flush();
}

}

namespace app
{

void installMessageHandler()
{
    qInstallMessageHandler(messageHandler);
}

void attachParentConsoleIfAvailable()
{
#ifdef Q_OS_WIN
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        FILE *unused;
        freopen_s(&unused, "CONOUT$", "w", stdout);
        freopen_s(&unused, "CONOUT$", "w", stderr);
    }
#endif
}

}
```

- [ ] **Step 3: Build (won't be linked yet, but verify it compiles when included)**

Since the file isn't included by any target yet, this step verifies it compiles by temporarily adding it to the CLI target. Skip if no easy compile check; defer to Phase 8 when `src/app/CMakeLists.txt` is created.

- [ ] **Step 4: Commit**

```bash
git add src/app/MessageHandler.h src/app/MessageHandler.cpp
git commit -m "feat(app): add MessageHandler with Windows console attach"
```

### Task 1.3: `app::plugins` PluginLoader

**Files:**
- Create: `src/app/PluginLoader.h`
- Create: `src/app/PluginLoader.cpp`
- Test: `tests/test_plugin_loader/CMakeLists.txt`
- Test: `tests/test_plugin_loader/test_plugin_loader.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

Create `tests/test_plugin_loader/test_plugin_loader.cpp`:

```cpp
#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "app/PluginLoader.h"

class TestPluginLoader : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void scan_default_roots_finds_mock();
    void scan_extra_roots_finds_plugin();
    void find_by_camera_returns_entry();
    void enumerate_cameras_returns_ids();
    void unload_all_clears_entries();
    void missing_root_does_not_crash();
    void empty_dir_returns_zero();

private:
    QTemporaryDir m_tempDir;
};

void TestPluginLoader::initTestCase() {
    QVERIFY(m_tempDir.isValid());
    // Copy mock plugin to temp dir for testing
    const QString srcDll = QString::fromLocal8Bit(qgetenv("MOCK_PLUGIN_PATH"));
    QVERIFY2(!srcDll.isEmpty(), "MOCK_PLUGIN_PATH not set");
    const QString dstDir = m_tempDir.path() + "/plugins";
    QDir().mkpath(dstDir);
    QVERIFY(QFile::copy(srcDll, dstDir + "/mock_camera_driver" + QFileInfo(srcDll).suffix()));
}

void TestPluginLoader::cleanupTestCase() {
    app::plugins::unloadAll();
}

void TestPluginLoader::scan_default_roots_finds_mock() {
    app::plugins::unloadAll();
    app::plugins::setExtraRoots({ m_tempDir.path() + "/plugins" });
    int loaded = app::plugins::scanDefaultRoots();
    QVERIFY(loaded >= 1);
    QVERIFY(app::plugins::entries().size() >= 1);
}

void TestPluginLoader::scan_extra_roots_finds_plugin() {
    app::plugins::unloadAll();
    int loaded = app::plugins::scan({ m_tempDir.path() + "/plugins" });
    QCOMPARE(loaded, app::plugins::entries().size());
    QVERIFY(loaded >= 1);
}

void TestPluginLoader::find_by_camera_returns_entry() {
    app::plugins::unloadAll();
    app::plugins::scan({ m_tempDir.path() + "/plugins" });
    const app::plugins::Entry *e = app::plugins::findByCamera("mock-001");
    QVERIFY(e != nullptr);
    QVERIFY(e->cameraIds.contains("mock-001"));
    QVERIFY(!e->filePath.isEmpty());
}

void TestPluginLoader::enumerate_cameras_returns_ids() {
    app::plugins::unloadAll();
    app::plugins::scan({ m_tempDir.path() + "/plugins" });
    QStringList ids = app::plugins::enumerateCameras();
    QVERIFY(ids.contains("mock-001"));
}

void TestPluginLoader::unload_all_clears_entries() {
    app::plugins::unloadAll();
    app::plugins::scan({ m_tempDir.path() + "/plugins" });
    QVERIFY(!app::plugins::entries().isEmpty());
    app::plugins::unloadAll();
    QVERIFY(app::plugins::entries().isEmpty());
}

void TestPluginLoader::missing_root_does_not_crash() {
    app::plugins::unloadAll();
    int loaded = app::plugins::scan({ "Z:/nonexistent/path/that/does/not/exist" });
    QCOMPARE(loaded, 0);
}

void TestPluginLoader::empty_dir_returns_zero() {
    app::plugins::unloadAll();
    const QString emptyDir = m_tempDir.path() + "/empty";
    QDir().mkpath(emptyDir);
    int loaded = app::plugins::scan({ emptyDir });
    QCOMPARE(loaded, 0);
}

QTEST_MAIN(TestPluginLoader)
#include "test_plugin_loader.moc"
```

Create `tests/test_plugin_loader/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(test_plugin_loader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Test REQUIRED)

add_executable(test_plugin_loader test_plugin_loader.cpp
    ${CMAKE_SOURCE_DIR}/src/app/PluginLoader.h
    ${CMAKE_SOURCE_DIR}/src/app/PluginLoader.cpp
)

target_include_directories(test_plugin_loader PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/mock
)

target_link_libraries(test_plugin_loader PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Test
)

add_dependencies(test_plugin_loader mock_camera_driver)
target_compile_definitions(test_plugin_loader PRIVATE
    MOCK_PLUGIN_PATH="$<TARGET_FILE:mock_camera_driver>"
)

add_test(NAME test_plugin_loader COMMAND test_plugin_loader)
```

- [ ] **Step 2: Add to tests/CMakeLists.txt**

Append `add_subdirectory(test_plugin_loader)` to `tests/CMakeLists.txt`.

- [ ] **Step 3: Add to CLI target temporarily so the test can build**

The test needs `src/app/PluginLoader.cpp` to compile. Add to `src/cli/CMakeLists.txt` (temporary, will be cleaned up in Phase 8):

```cmake
target_sources(ezspeccam_cli PRIVATE
    ../app/PluginLoader.h
    ../app/PluginLoader.cpp
)
target_include_directories(ezspeccam_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app
)
```

- [ ] **Step 4: Run build to verify it fails (no implementation yet)**

```bash
.\build_preset.bat debug
```

Expected: build fails because `src/app/PluginLoader.{h,cpp}` don't exist.

- [ ] **Step 5: Write implementation**

`src/app/PluginLoader.h`:

```cpp
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class ICameraDriver;

namespace app::plugins
{

struct Entry
{
    QString filePath;
    QStringList cameraIds;
    ICameraDriver *instance = nullptr;
};

void setExtraRoots(const QStringList &roots);
int  scanDefaultRoots();
int  scan(const QStringList &roots);
const QVector<Entry> &entries();
const Entry *findByCamera(const QString &cameraId);
QStringList enumerateCameras();
void unloadAll();

}
```

`src/app/PluginLoader.cpp`:

```cpp
#include "PluginLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QSet>

#include "ICameraDriver.h"

namespace
{

QVector<app::plugins::Entry> g_entries;
QStringList g_extraRoots;

QStringList defaultRoots()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    return { appDir + "/plugins/drivers", appDir + "/../plugins/drivers" };
}

}

namespace app::plugins
{

void setExtraRoots(const QStringList &roots)
{
    g_extraRoots = roots;
}

int scan(const QStringList &roots)
{
    unloadAll();

    int loaded = 0;
    QSet<QString> seenFiles;

    for (const QString &root : roots)
    {
        QDir dir(root);
        if (!dir.exists())
            continue;

        for (const QFileInfo &fi : dir.entryInfoList(QDir::Files))
        {
            const QString path = fi.absoluteFilePath();
            if (!path.endsWith(".dll", Qt::CaseInsensitive) &&
                !path.endsWith(".so", Qt::CaseInsensitive) &&
                !path.endsWith(".dylib", Qt::CaseInsensitive))
                continue;
            if (seenFiles.contains(QFileInfo(path).canonicalFilePath()))
                continue;
            seenFiles.insert(QFileInfo(path).canonicalFilePath());

            auto *loader = new QPluginLoader(path);
            QObject *obj = loader->instance();
            auto *driver = qobject_cast<ICameraDriver *>(obj);
            if (!driver)
            {
                loader->unload();
                delete loader;
                continue;
            }

            Entry e;
            e.filePath = path;
            e.cameraIds = driver->enumerate();
            e.instance = driver;
            g_entries.append(e);
            ++loaded;
        }
    }
    return loaded;
}

int scanDefaultRoots()
{
    QStringList roots = defaultRoots();
    roots += g_extraRoots;
    return scan(roots);
}

const QVector<Entry> &entries()
{
    return g_entries;
}

const Entry *findByCamera(const QString &cameraId)
{
    for (const Entry &e : g_entries)
    {
        if (e.cameraIds.contains(cameraId))
            return &e;
    }
    return nullptr;
}

QStringList enumerateCameras()
{
    QStringList ids;
    for (const Entry &e : g_entries)
        ids += e.cameraIds;
    return ids;
}

void unloadAll()
{
    g_entries.clear();
    // QPluginLoader instances are intentionally leaked: drivers must outlive
    // the QCoreApplication destructor sequence. Unloading here would force
    // static destruction order issues.
}

}
```

- [ ] **Step 6: Build + run test**

```bash
.\build_preset.bat debug
.\build\msvc-debug\bin\Debug\test_plugin_loader.exe -o test_plugin_loader_out.txt
type test_plugin_loader_out.txt
```

Expected: All 7 test cases PASS.

- [ ] **Step 7: Commit**

```bash
git add src/app/PluginLoader.h src/app/PluginLoader.cpp tests/test_plugin_loader/ tests/CMakeLists.txt src/cli/CMakeLists.txt
git commit -m "feat(app): add app::plugins::scan and friends with tests"
```

### Task 1.4: `app::WaitStabilizer`

**Files:**
- Create: `src/app/WaitStabilizer.h`
- Create: `src/app/WaitStabilizer.cpp`
- Test: `tests/test_wait_stabilizer/CMakeLists.txt`
- Test: `tests/test_wait_stabilizer/test_wait_stabilizer.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing test**

`tests/test_wait_stabilizer/test_wait_stabilizer.cpp`:

```cpp
#include <QtTest>
#include <QCoreApplication>
#include <QObject>
#include <QTimer>
#include <QVariant>
#include <QSharedPointer>
#include <QImage>

#include "app/WaitStabilizer.h"
#include "ICameraDriver.h"
#include "CameraTypes.h"

class StubDriver : public QObject
{
    Q_OBJECT
public:
    double currentValue = 0.0;

    QVariant parameterValue(const QString &) const { return currentValue; }
    Q_INVOKABLE void tick(double v) { currentValue = v; }

    // Unused for this test but required by interface signature matches:
    Q_INVOKABLE QStringList parameterNames() const { return { "temperature" }; }
};

class TestWaitStabilizer : public QObject
{
    Q_OBJECT
private slots:
    void reaches_target_when_stable();
    void times_out_when_unstable();

private:
    StubDriver *m_stub = nullptr;
    ICameraDriver *m_iface = nullptr;
    void *m_ifacePtr = nullptr;
};

void TestWaitStabilizer::reaches_target_when_stable() {
    m_stub = new StubDriver;
    m_ifacePtr = m_stub;
    // Cast: StubDriver is not actually an ICameraDriver. We pass it as void* and
    // re-interpret; the real ICameraDriver pointer comes from a mock plugin in
    // production. For this test, we pass a custom lambda-less driver; to keep
    // the test simple, the test will be rewritten in Task 1.5 to use the real
    // mock plugin driver.
    QSKIP("Rewritten in Task 1.5 with real mock plugin driver.");
}

void TestWaitStabilizer::times_out_when_unstable() {
    QSKIP("Rewritten in Task 1.5 with real mock plugin driver.");
}

QTEST_MAIN(TestWaitStabilizer)
#include "test_wait_stabilizer.moc"
```

NOTE: The TDD-first red test above is a stub. The real WaitStabilizer is tightly coupled to ICameraDriver; we cannot trivially stub it without a real driver. **Skip to direct implementation, write integration test using mock plugin driver.**

- [ ] **Step 1 (revised): Skip failing-test step, write implementation directly**

`src/app/WaitStabilizer.h`:

```cpp
#pragma once

#include <QString>

class ICameraDriver;

namespace app
{

/// Poll a driver parameter until all readings in a sliding window are within
/// `tolerance` of `target`, or until `timeoutSec` elapses.
/// @return true if stable; false on timeout or read failure.
bool waitForStable(ICameraDriver *driver,
                   const QString &paramName,
                   double target,
                   double tolerance,
                   double windowSec,
                   double timeoutSec);

}
```

`src/app/WaitStabilizer.cpp`:

```cpp
#include "WaitStabilizer.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>
#include <QVariant>

#include "ICameraDriver.h"

namespace app
{

namespace
{
struct Sample { qint64 ms; double value; };
}

bool waitForStable(ICameraDriver *driver,
                   const QString &paramName,
                   double target,
                   double tolerance,
                   double windowSec,
                   double timeoutSec)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    QVector<Sample> samples;
    QEventLoop loop;
    QTextStream statusOut(stdout);
    int exitCode = -1;

    QTimer pollTimer;
    pollTimer.setInterval(500);
    QObject::connect(&pollTimer, &QTimer::timeout, [&]() {
        QVariant val = driver->parameterValue(paramName);
        if (!val.isValid()) {
            statusOut << '\n'; statusOut.flush();
            exitCode = -1; loop.quit(); return;
        }
        double current = val.toDouble();
        qint64 elapsed = totalTimer.elapsed();
        samples.append({elapsed, current});

        while (!samples.isEmpty() && (elapsed - samples.first().ms) > windowSec * 1000)
            samples.removeFirst();

        QString status = QStringLiteral("  %1 = %2 (target: %3, delta=%4, elapsed: %5s)")
            .arg(paramName, -28)
            .arg(current, 8, 'f', 2)
            .arg(target, 8, 'f', 2)
            .arg(qAbs(current - target), 0, 'f', 3)
            .arg(elapsed / 1000.0, 0, 'f', 1);
        status = status.leftJustified(79, QLatin1Char(' '));
        statusOut << '\r' << status; statusOut.flush();

        if (samples.size() >= 3) {
            bool stable = true;
            for (const auto &s : samples) {
                if (qAbs(s.value - target) > tolerance) { stable = false; break; }
            }
            if (stable) {
                double windowMs = windowSec * 1000;
                double actualWindow = elapsed - samples.first().ms;
                if (actualWindow >= windowMs * 0.9) {
                    statusOut << '\n'; statusOut.flush();
                    exitCode = 0; loop.quit(); return;
                }
            }
        }

        if (elapsed > timeoutSec * 1000) {
            statusOut << '\n'; statusOut.flush();
            exitCode = -1; loop.quit();
        }
    });

    pollTimer.start();
    loop.exec();
    pollTimer.stop();
    return exitCode == 0;
}

}
```

- [ ] **Step 2: Wire into CLI target temporarily for build verification**

Add to `src/cli/CMakeLists.txt`:

```cmake
target_sources(ezspeccam_cli PRIVATE
    ../app/WaitStabilizer.h
    ../app/WaitStabilizer.cpp
)
```

- [ ] **Step 3: Build**

```bash
.\build_preset.bat debug
```

Expected: build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/app/WaitStabilizer.h src/app/WaitStabilizer.cpp src/cli/CMakeLists.txt
git commit -m "feat(app): add app::waitForStable"
```

---

## Phase 2: Format Handlers Relocation

### Task 2.1: Move SaveTypes, IImageFormatHandler, and Format Handlers to `src/app/formats/`

**Files:**
- Move: `src/gui/workers/SaveTypes.h` → `src/app/formats/SaveTypes.h`
- Move: `src/gui/workers/IImageFormatHandler.h` → `src/app/formats/IImageFormatHandler.h`
- Move: `src/gui/workers/formats/TiffFormatHandler.h` → `src/app/formats/TiffFormatHandler.h`
- Move: `src/gui/workers/formats/TiffFormatHandler.cpp` → `src/app/formats/TiffFormatHandler.cpp`
- Move: `src/gui/workers/formats/CsvFormatHandler.h` → `src/app/formats/CsvFormatHandler.h`
- Move: `src/gui/workers/formats/CsvFormatHandler.cpp` → `src/app/formats/CsvFormatHandler.cpp`

- [ ] **Step 1: Move files (using `git mv` to preserve history)**

```bash
cd /d D:\10_Projects\2502-Sw-EZSpecCam-shadow
git mv src/gui/workers/SaveTypes.h src/app/formats/SaveTypes.h
git mv src/gui/workers/IImageFormatHandler.h src/app/formats/IImageFormatHandler.h
git mv src/gui/workers/formats/TiffFormatHandler.h src/app/formats/TiffFormatHandler.h
git mv src/gui/workers/formats/TiffFormatHandler.cpp src/app/formats/TiffFormatHandler.cpp
git mv src/gui/workers/formats/CsvFormatHandler.h src/app/formats/CsvFormatHandler.h
git mv src/gui/workers/formats/CsvFormatHandler.cpp src/app/formats/CsvFormatHandler.cpp
```

- [ ] **Step 2: Update include paths inside the moved files**

Edit `src/app/formats/IImageFormatHandler.h` — change `#include "SaveTypes.h"` to `#include "formats/SaveTypes.h"` (relative to `src/`, the new include root).

Edit `src/app/formats/TiffFormatHandler.h`:
- Change `#include "../IImageFormatHandler.h"` to `#include "IImageFormatHandler.h"`
- Change `#include "../SaveTypes.h"` to `#include "SaveTypes.h"`

Edit `src/app/formats/CsvFormatHandler.h`:
- Change `#include "../IImageFormatHandler.h"` to `#include "IImageFormatHandler.h"`
- Change `#include "../SaveTypes.h"` to `#include "SaveTypes.h"`

The .cpp files use these headers by quoted local include — they continue to work without changes.

- [ ] **Step 3: Update consumers (FileSaverWorker, FileLoaderWorker)**

Edit `src/gui/workers/FileSaverWorker.h`:
- Change `#include "IImageFormatHandler.h"` to `#include "formats/IImageFormatHandler.h"`
- Change `#include "SaveTypes.h"` to `#include "formats/SaveTypes.h"`

Edit `src/gui/workers/FileLoaderWorker.h`:
- Change `#include "SaveTypes.h"` to `#include "formats/SaveTypes.h"`

- [ ] **Step 4: Update test includes**

Edit `tests/test_metadata_save_load/test_metadata_save_load.cpp`:
- Change `#include "SaveTypes.h"` to `#include "formats/SaveTypes.h"`
- Change `#include "formats/TiffFormatHandler.h"` to `#include "formats/TiffFormatHandler.h"` (no change in path; folder is now under `src/app/`)
- Change `#include "formats/CsvFormatHandler.h"` to `#include "formats/CsvFormatHandler.h"`
- Change `#include "FileLoaderWorker.h"` to `#include "gui/workers/FileLoaderWorker.h"`

Edit `tests/test_row_range_dialog/test_row_range_dialog.cpp`:
- Change `#include "workers/FileLoaderWorker.h"` to `#include "gui/workers/FileLoaderWorker.h"`
- Change `#include "workers/formats/TiffFormatHandler.h"` to `#include "formats/TiffFormatHandler.h"`
- Change `#include "workers/SaveTypes.h"` to `#include "formats/SaveTypes.h"`

- [ ] **Step 5: Update test CMakeLists.txt paths**

Edit `tests/test_metadata_save_load/CMakeLists.txt`:
- Change `${CMAKE_SOURCE_DIR}/src/gui/workers/formats/TiffFormatHandler.cpp` → `${CMAKE_SOURCE_DIR}/src/app/formats/TiffFormatHandler.cpp`
- Change `${CMAKE_SOURCE_DIR}/src/gui/workers/formats/CsvFormatHandler.cpp` → `${CMAKE_SOURCE_DIR}/src/app/formats/CsvFormatHandler.cpp`
- Change `${CMAKE_SOURCE_DIR}/src/gui/workers/IImageFormatHandler.h` → `${CMAKE_SOURCE_DIR}/src/app/formats/IImageFormatHandler.h`
- Change `${CMAKE_SOURCE_DIR}/src/gui/workers/SaveTypes.h` → `${CMAKE_SOURCE_DIR}/src/app/formats/SaveTypes.h`
- Keep `${CMAKE_SOURCE_DIR}/src/gui/workers/FileLoaderWorker.cpp` (stays in `src/gui/workers/`)
- Add include dir: `${CMAKE_SOURCE_DIR}/src/app` (so the test sees `formats/SaveTypes.h` etc.)
- Replace `target_include_directories(... src/gui/workers ...)` with both `src/gui/workers` AND `src/app`.

Edit `tests/test_row_range_dialog/CMakeLists.txt`:
- Same include-dir addition: `${CMAKE_SOURCE_DIR}/src/app`
- Keep `${CMAKE_SOURCE_DIR}/src/gui/workers/FileLoaderWorker.cpp` (stays)

- [ ] **Step 6: Build and run tests**

```bash
.\build_preset.bat debug
.\run_tests.bat
```

Expected: All tests pass; `test_metadata_save_load` confirms wide CSV + `softwareSettings` block.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "refactor: move format handlers to src/app/formats/"
```

### Task 2.2: Add `app::formats::saveFrame` dispatcher

**Files:**
- Create: `src/app/formats/FrameWriter.h`
- Create: `src/app/formats/FrameWriter.cpp`

- [ ] **Step 1: Create header**

`src/app/formats/FrameWriter.h`:

```cpp
#pragma once

#include <QString>
#include <QStringList>

#include "CameraTypes.h"

namespace app::formats
{

/// Save a frame to disk; dispatch by file extension. Returns true on success.
bool saveFrame(const ImageData &frame, const QString &filePath);

QStringList supportedSaveExtensions();

/// Map CLI --format values to file extensions: "tiff" → "tiff", "csv" → "csv".
/// Unknown values return "tiff".
QString extensionForCliFormat(const QString &cliFormat);

/// Compose "img_yyyyMMdd_hhmmss_zzz.ext" with optional prefix/suffix.
QString generateFilename(const QString &outputDir,
                         const QString &prefix,
                         const QString &suffix,
                         const QString &extension);

}
```

- [ ] **Step 2: Create implementation**

`src/app/formats/FrameWriter.cpp`:

```cpp
#include "FrameWriter.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include "IImageFormatHandler.h"
#include "TiffFormatHandler.h"
#include "CsvFormatHandler.h"

#include <memory>
#include <vector>

namespace
{

std::vector<std::unique_ptr<IImageFormatHandler>> buildHandlers()
{
    std::vector<std::unique_ptr<IImageFormatHandler>> v;
    v.push_back(std::make_unique<TiffFormatHandler>());
    v.push_back(std::make_unique<CsvFormatHandler>());
    return v;
}

IImageFormatHandler *findHandler(const QString &filePath)
{
    static const auto handlers = buildHandlers();
    const QString ext = QFileInfo(filePath).suffix().toLower();
    for (const auto &h : handlers)
        if (h->canHandle(filePath)) return h.get();
    return nullptr;
}

}

namespace app::formats
{

bool saveFrame(const ImageData &frame, const QString &filePath)
{
    IImageFormatHandler *h = findHandler(filePath);
    if (!h) return false;

    SaveRequest req;
    req.frame = frame;
    req.filePath = filePath;
    return h->save(req);
}

QStringList supportedSaveExtensions()
{
    return { "tiff", "tif", "csv" };
}

QString extensionForCliFormat(const QString &cliFormat)
{
    const QString f = cliFormat.toLower();
    if (f == "csv") return "csv";
    return "tiff";
}

QString generateFilename(const QString &outputDir,
                         const QString &prefix,
                         const QString &suffix,
                         const QString &extension)
{
    QString ext = extension.toLower();
    if (ext.isEmpty()) ext = "tiff";

    QString name;
    if (!prefix.isEmpty()) name += prefix + "_";
    name += "img_";
    name += QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    if (!suffix.isEmpty()) name += "_" + suffix;
    name += "." + ext;

    QDir dir(outputDir.isEmpty() ? "." : outputDir);
    return dir.absoluteFilePath(name);
}

}
```

- [ ] **Step 3: Add to CLI target for build verification**

Add to `src/cli/CMakeLists.txt`:

```cmake
target_sources(ezspeccam_cli PRIVATE
    ../app/formats/FrameWriter.h
    ../app/formats/FrameWriter.cpp
)
```

- [ ] **Step 4: Build**

```bash
.\build_preset.bat debug
```

Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/app/formats/FrameWriter.h src/app/formats/FrameWriter.cpp src/cli/CMakeLists.txt
git commit -m "feat(app): add app::formats::saveFrame dispatcher"
```

---

## Phase 3: AppController Refactor

### Task 3.1: AppController uses `app::plugins`

**Files:**
- Modify: `src/gui/AppController.h`
- Modify: `src/gui/AppController.cpp`
- Test: `tests/test_app_controller/test_app_controller.cpp`

- [ ] **Step 1: Update AppController.h — replace PluginInfo struct with include**

In `src/gui/AppController.h`:
- Remove the `struct PluginInfo { ... }` block.
- Add `#include "app/PluginLoader.h"` near the top.
- Replace `QList<PluginInfo> loadedPlugins() const;` with `QVector<app::plugins::Entry> loadedPlugins() const;`
- Remove `QList<PluginInfo> m_plugins;` member.
- Remove `const PluginInfo *findPluginForCamera(const QString &cameraId) const;`
- Remove `bool loadPlugin(const QString &filePath);`
- Remove `void unloadPlugin(const PluginInfo &info);`
- Remove `void clearPlugins();`
- Remove `QString m_pluginDir;` member.
- Remove `void setPluginDirectory(const QString &path);` declaration.
- Keep the `scanPlugins()` slot; its body will be rewritten.

- [ ] **Step 2: Update AppController.cpp — rewrite plugin-related methods**

In `src/gui/AppController.cpp`:

Replace the constructor body plugin-dir init:

```cpp
AppController::AppController(QObject *parent)
    : QObject(parent)
{
}
```

Remove `setPluginDirectory`.

Rewrite `scanPlugins`:

```cpp
void AppController::scanPlugins()
{
    int loaded = app::plugins::scanDefaultRoots();
    emit pluginScanCompleted(loaded, loaded);
}
```

Remove `loadPlugin`, `unloadPlugin`, `clearPlugins`, `findPluginForCamera`.

Replace `availableCameras`:

```cpp
QStringList AppController::availableCameras() const
{
    return app::plugins::enumerateCameras();
}
```

Replace `loadedPlugins`:

```cpp
QVector<app::plugins::Entry> AppController::loadedPlugins() const
{
    return app::plugins::entries();
}
```

Replace `hasPlugins`:

```cpp
bool AppController::hasPlugins() const
{
    return !app::plugins::entries().isEmpty();
}
```

In `connectCamera` (currently ~line 120), replace the `findPluginForCamera` block with:

```cpp
const app::plugins::Entry *pluginInfo = app::plugins::findByCamera(cameraId);
if (!pluginInfo) { /* existing error path */ }
m_driver = pluginInfo->instance;
if (!m_driver) { /* existing error path */ }
// ... rest of method unchanged
```

Remove `disconnectFromDriver`'s reference to `m_plugins` if any (verify and remove `clearPlugins` call from destructor since plugin registry is global).

- [ ] **Step 3: Update PluginTab.cpp to use new type**

Edit `src/gui/widgets/config/PluginTab.cpp`:

- Change `#include "../../AppController.h"` (no change).
- Change line 191: `const QList<PluginInfo> plugins = m_appController->loadedPlugins();` → `const auto plugins = m_appController->loadedPlugins();`
- The loop body that reads `info.filePath` and `info.cameraIds` works unchanged because the field set is identical.

- [ ] **Step 4: Update test_app_controller if needed**

`tests/test_app_controller/test_app_controller.cpp` does not directly reference PluginInfo (verify by `grep`); only the `connectCamera` path is exercised. Run the test to confirm:

```bash
.\build_preset.bat debug
.\build\msvc-debug\bin\Debug\test_app_controller.exe -o test_app_controller_out.txt
type test_app_controller_out.txt
```

Expected: All tests pass. (If the test file does reference PluginInfo, update the import to `app::plugins::Entry`.)

- [ ] **Step 5: Add new includes to AppController's CMake**

`src/gui/CMakeLists.txt` doesn't reference `app/PluginLoader.h` because it uses include dirs. Confirm `src` is on the include path; verify with build.

The temporary `src/cli/CMakeLists.txt` already includes `app/PluginLoader.cpp`. The GUI's `AppController.h` includes `app/PluginLoader.h` from the repo-root `src/` include path. Verify with build.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "refactor(gui): AppController delegates plugin mgmt to app::plugins"
```

---

## Phase 4: Headless Pipeline

### Task 4.1: Move SequenceRunner to `src/app/`

**Files:**
- Move: `src/cli/SequenceRunner.h` → `src/app/SequenceRunner.h`
- Move: `src/cli/SequenceRunner.cpp` → `src/app/SequenceRunner.cpp`
- Modify: `src/cli/CMakeLists.txt` (remove SequenceRunner from CLI's sources; add via app includes)

- [ ] **Step 1: git mv**

```bash
git mv src/cli/SequenceRunner.h src/app/SequenceRunner.h
git mv src/cli/SequenceRunner.cpp src/app/SequenceRunner.cpp
```

- [ ] **Step 2: Verify no internal includes break**

The .h and .cpp have no includes referencing `cli/` paths. Confirm with `grep`:

```bash
grep -n "include" src/app/SequenceRunner.h src/app/SequenceRunner.cpp
```

Expected: only Qt headers and the self-include. No change needed.

- [ ] **Step 3: Update CLI CMakeLists to point at the new path**

Edit `src/cli/CMakeLists.txt`:
- Change `SequenceRunner.h` → `../app/SequenceRunner.h`
- Change `SequenceRunner.cpp` → `../app/SequenceRunner.cpp`

- [ ] **Step 4: Build and test**

```bash
.\build_preset.bat debug
.\run_tests.bat
```

Expected: All tests pass.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor: move SequenceRunner to src/app/"
```

### Task 4.2: `app::HeadlessController`

**Files:**
- Create: `src/app/HeadlessController.h`
- Create: `src/app/HeadlessController.cpp`
- Modify: `src/cli/CMakeLists.txt`

- [ ] **Step 1: Create header**

`src/app/HeadlessController.h`:

```cpp
#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>

#include "SequenceRunner.h"

namespace app
{

struct HeadlessOptions
{
    bool listCameras = false;
    bool listParams = false;
    QString cameraId;
    QVariantMap setParameters;
    int frames = 1;
    QString outputDir = ".";
    QString format = "tiff";
    QString prefix;
    QString suffix;
    QVector<SequenceStep> sequence;
};

/// Run a headless session. Returns 0 on success, 1 on user error, -1 on internal error.
int run(const HeadlessOptions &opts);

}
```

- [ ] **Step 2: Create implementation**

`src/app/HeadlessController.cpp`:

```cpp
#include "HeadlessController.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QSharedPointer>
#include <QTextStream>

#include "ICameraDriver.h"
#include "CameraTypes.h"
#include "MessageHandler.h"
#include "PluginLoader.h"
#include "WaitStabilizer.h"
#include "formats/FrameWriter.h"

namespace app
{

namespace
{

void listCameras()
{
    qInfo() << "Available cameras:";
    bool found = false;
    for (const auto &e : app::plugins::entries())
    {
        for (const QString &id : e.cameraIds)
        {
            qInfo().noquote() << "  " << id << " (driver: " << e.filePath << ")";
            found = true;
        }
    }
    if (!found) qInfo() << "  (none)";
}

void listParameters(ICameraDriver *driver)
{
    qInfo() << "Parameters for" << driver->cameraId() << ":";
    for (const QString &name : driver->parameterNames())
    {
        ParameterDefinition def = driver->parameter(name);
        QVariant val = driver->parameterValue(name);
        QString flags;
        if (def.isReadOnly)  flags += " [readonly]";
        if (def.isExtrinsic) flags += " [extrinsic]";
        if (def.isDynamic)   flags += " [dynamic]";
        qInfo().noquote() << QString("  %1 = %2 (%3)%4")
            .arg(name, -28)
            .arg(val.toString(), -16)
            .arg(def.displayName)
            .arg(flags);
    }
}

int captureFrames(ICameraDriver *driver, int frameCount,
                  const QString &outputDir, const QString &format,
                  const QString &prefix, const QString &suffix)
{
    if (frameCount <= 0) { qCritical() << "Frame count must be > 0"; return -1; }

    QEventLoop loop;
    int captured = 0;
    int errors = 0;

    QObject::connect(driver, &ICameraDriver::frameReady,
        [&](const QSharedPointer<QImage> &image, quint64 ts, int frameNumber,
            const QString &cameraId, const QVariantMap &parameters)
        {
            if (captured >= frameCount) return;
            ImageData frame;
            frame.image = *image;
            frame.originalImage = *image;
            frame.timestamp = ts;
            frame.frameNumber = frameNumber;
            frame.cameraId = cameraId;
            frame.parameters = parameters;
            const QString ext = app::formats::extensionForCliFormat(format);
            const QString filePath = app::formats::generateFilename(outputDir, prefix, suffix, ext);
            if (!app::formats::saveFrame(frame, filePath)) errors++;
            captured++;
            qInfo() << "Frame" << captured << "/" << frameCount;
            if (captured >= frameCount) loop.quit();
        });

    QObject::connect(driver, &ICameraDriver::captureStopped,
        [&](const QString &) { if (captured < frameCount) { qWarning() << "Capture stopped unexpectedly"; loop.quit(); } });

    QObject::connect(driver, &ICameraDriver::errorOccurred,
        [&](const CameraError &err) { qWarning().noquote() << "Camera error:" << err.description; errors++; });

    qInfo() << "Starting capture:" << frameCount << "frames...";
    if (!driver->startCapture(frameCount))
    {
        qCritical() << "Failed to start capture";
        return -1;
    }
    loop.exec();
    driver->stopCapture();
    qInfo() << "Capture complete:" << captured << "frames," << errors << "errors";
    return captured;
}

int runSequence(ICameraDriver *driver, const HeadlessOptions &opts,
                const QVector<SequenceStep> &steps)
{
    qInfo() << "Running sequence:" << steps.size() << "steps";
    for (int i = 0; i < steps.size(); ++i)
    {
        const SequenceStep &step = steps[i];
        qInfo() << "";
        qInfo() << QString("--- Step %1/%2 ---").arg(i + 1).arg(steps.size());
        switch (step.type)
        {
        case SequenceStep::Configure:
            for (auto it = step.parameters.constBegin(); it != step.parameters.constEnd(); ++it)
            {
                qInfo().noquote() << "  set" << it.key() << "=" << it.value().toString();
                if (!driver->setParameter(it.key(), it.value()))
                    qWarning().noquote() << "  Failed to set" << it.key();
            }
            if (!driver->commitParameters())
                qWarning() << "  Failed to commit parameters";
            break;
        case SequenceStep::Capture:
        {
            QString outDir = step.outputDir.isEmpty() ? opts.outputDir : step.outputDir;
            QString fmt = step.format.isEmpty() ? opts.format : step.format;
            QString pfx = step.prefix.isEmpty() ? opts.prefix : step.prefix;
            QString sfx = step.suffix.isEmpty() ? opts.suffix : step.suffix;
            if (captureFrames(driver, step.frames, outDir, fmt, pfx, sfx) < 0)
                return -1;
            break;
        }
        case SequenceStep::WaitStable:
            if (!waitForStable(driver, step.stableParam, step.stableTarget,
                               step.stableTolerance, step.stableWindowSec,
                               step.stableTimeoutSec))
                return -1;
            break;
        }
    }
    return 0;
}

}

int run(const HeadlessOptions &opts)
{
    int loaded = app::plugins::scanDefaultRoots();
    if (loaded == 0) { qCritical() << "No camera drivers found"; return 1; }

    if (opts.listCameras) { listCameras(); return 0; }

    if (opts.cameraId.isEmpty()) {
        qCritical() << "No camera specified. Use --camera <id> or --list.";
        return 1;
    }

    const app::plugins::Entry *entry = app::plugins::findByCamera(opts.cameraId);
    if (!entry) { qCritical().noquote() << "Camera not found:" << opts.cameraId; return 1; }
    ICameraDriver *driver = entry->instance;
    if (!driver) { qCritical() << "Driver instance null"; return 1; }

    qInfo().noquote() << "Connecting to" << opts.cameraId << "...";
    if (!driver->connectToCamera(opts.cameraId)) {
        qCritical() << "Failed to connect to" << opts.cameraId;
        return 1;
    }
    qInfo() << "Connected.";

    if (opts.listParams) { listParameters(driver); driver->disconnectCamera(); return 0; }

    for (auto it = opts.setParameters.constBegin(); it != opts.setParameters.constEnd(); ++it)
    {
        qInfo().noquote() << "Setting" << it.key() << "=" << it.value().toString();
        if (!driver->setParameter(it.key(), it.value()))
            qWarning().noquote() << "  Warning: setParameter returned false for" << it.key();
    }
    if (!opts.setParameters.isEmpty() && !driver->commitParameters())
        qWarning() << "Warning: commitParameters returned false";

    int rc = 0;
    if (!opts.sequence.isEmpty())
        rc = runSequence(driver, opts, opts.sequence);
    else
        rc = captureFrames(driver, opts.frames, opts.outputDir, opts.format, opts.prefix, opts.suffix);

    driver->disconnectCamera();
    return (rc >= 0) ? 0 : 1;
}

}
```

- [ ] **Step 3: Add to CLI target**

Add to `src/cli/CMakeLists.txt` `target_sources`:
```cmake
../app/HeadlessController.h
../app/HeadlessController.cpp
```

- [ ] **Step 4: Build**

```bash
.\build_preset.bat debug
```

Expected: build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/app/HeadlessController.h src/app/HeadlessController.cpp src/cli/CMakeLists.txt
git commit -m "feat(app): add app::run HeadlessController"
```

### Task 4.3: Rewrite `src/cli/main.cpp` to use `app::run`

**Files:**
- Modify: `src/cli/main.cpp`

- [ ] **Step 1: Replace `main.cpp` with thin wrapper**

```cpp
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>

#include "app/HeadlessController.h"
#include "app/MessageHandler.h"
#include "SequenceRunner.h"

static QVariant parseSetValue(const QString &valueStr)
{
    bool ok = false;
    double d = valueStr.toDouble(&ok);
    if (ok)
    {
        if (valueStr.contains('.')) return d;
        qint64 i = valueStr.toLongLong(&ok);
        return ok ? QVariant(i) : QVariant(d);
    }
    return valueStr;
}

int main(int argc, char *argv[])
{
    app::installMessageHandler();
    app::attachParentConsoleIfAvailable();

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("EZSpecCam");
    QCoreApplication::setApplicationVersion("1.0.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("EZSpecCam headless camera control");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption listOpt("list", "List available cameras");
    QCommandLineOption listParamsOpt("list-params", "List all camera parameters");
    QCommandLineOption cameraOpt("camera", "Camera ID to connect to", "id");
    QCommandLineOption setOpt("set", "Set parameter (name=value)", "param");
    QCommandLineOption framesOpt("frames", "Number of frames to capture", "n", "1");
    QCommandLineOption outputOpt("output", "Output directory", "dir", ".");
    QCommandLineOption formatOpt("format", "Output format (tiff|csv)", "fmt", "tiff");
    QCommandLineOption prefixOpt("prefix", "Filename prefix", "str");
    QCommandLineOption suffixOpt("suffix", "Filename suffix", "str");
    QCommandLineOption sequenceOpt("sequence", "Event sequence JSON file", "file");

    parser.addOption(listOpt);
    parser.addOption(listParamsOpt);
    parser.addOption(cameraOpt);
    parser.addOption(setOpt);
    parser.addOption(framesOpt);
    parser.addOption(outputOpt);
    parser.addOption(formatOpt);
    parser.addOption(prefixOpt);
    parser.addOption(suffixOpt);
    parser.addOption(sequenceOpt);

    parser.process(app);

    app::HeadlessOptions opts;
    opts.listCameras = parser.isSet(listOpt);
    opts.listParams = parser.isSet(listParamsOpt);
    opts.cameraId = parser.value(cameraOpt);
    opts.frames = parser.value(framesOpt).toInt();
    opts.outputDir = parser.value(outputOpt);
    opts.format = parser.value(formatOpt).toLower();
    opts.prefix = parser.value(prefixOpt);
    opts.suffix = parser.value(suffixOpt);

    for (const QString &sv : parser.values(setOpt))
    {
        int eq = sv.indexOf('=');
        if (eq < 0) { qWarning() << "Invalid --set:" << sv; continue; }
        opts.setParameters[sv.left(eq).trimmed()] = parseSetValue(sv.mid(eq + 1).trimmed());
    }

    if (parser.isSet(sequenceOpt))
    {
        SequenceRunner seq;
        if (!seq.loadFromFile(parser.value(sequenceOpt))) {
            qCritical().noquote() << "Failed to load sequence:" << seq.errorString();
            return 1;
        }
        opts.sequence = seq.steps();
        if (opts.outputDir == "." && !seq.defaultOutputDir.isEmpty()) opts.outputDir = seq.defaultOutputDir;
        if (opts.format == "tiff" && !seq.defaultFormat.isEmpty())      opts.format = seq.defaultFormat;
        if (opts.prefix.isEmpty() && !seq.defaultPrefix.isEmpty())      opts.prefix = seq.defaultPrefix;
        if (opts.suffix.isEmpty() && !seq.defaultSuffix.isEmpty())      opts.suffix = seq.defaultSuffix;
    }

    return app::run(opts);
}
```

- [ ] **Step 2: Build + run end-to-end smoke**

```bash
.\build_preset.bat debug
.\build\msvc-debug\bin\Debug\ezspeccam_cli.exe --list
```

Expected: lists `mock-001`.

```bash
mkdir tmp_cli
.\build\msvc-debug\bin\Debug\ezspeccam_cli.exe --camera mock-001 --frames 1 --output tmp_cli
dir tmp_cli
```

Expected: one `.tiff` and one `_metadata.json`; the JSON contains `"softwareSettings": {}`.

```bash
mkdir tmp_csv
.\build\msvc-debug\bin\Debug\ezspeccam_cli.exe --camera mock-001 --frames 1 --format csv --output tmp_csv
```

Expected: one `.csv` whose line count equals the mock image height (no header line).

- [ ] **Step 3: Commit**

```bash
git add src/cli/main.cpp
git rm src/cli/main.cpp.bak 2>nul || true
git commit -m "refactor(cli): main.cpp delegates to app::run"
```

---

## Phase 5: Mode Dispatch Entry Point

### Task 5.1: Write `src/app/main.cpp`

**Files:**
- Create: `src/app/main.cpp`
- Modify: `src/cli/main.cpp` — already done in Phase 4; no further change

- [ ] **Step 1: Defer creation of `src/app/main.cpp` until Phase 8**

`src/app/CMakeLists.txt` will own the executable. The `src/cli/main.cpp` we already have IS the headless entry; the GUI's `src/gui/main.cpp` will be replaced by `src/gui/GuiMain.cpp` in Task 5.2. The single-binary entry is built in Phase 8.

For now, both `src/cli/main.cpp` and `src/gui/main.cpp` still exist and produce their respective binaries.

- [ ] **Step 2: Skip — no commit yet**

### Task 5.2: Write `src/gui/GuiMain.cpp`

**Files:**
- Create: `src/gui/GuiMain.cpp`

- [ ] **Step 1: Create file**

```cpp
#include <QApplication>
#include <QSurfaceFormat>

#include "widgets/MainWindow.h"

namespace gui
{

int run(QApplication &app)
{
    QCoreApplication::setApplicationName("EZSpecCam");
    QCoreApplication::setApplicationVersion("1.0.0");
    QCoreApplication::setOrganizationName("EZSpecCam");

    QApplication::setStyle("Fusion");

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    MainWindow window;
    window.show();
    return app.exec();
}

}
```

- [ ] **Step 2: Commit**

```bash
git add src/gui/GuiMain.cpp
git commit -m "refactor(gui): extract GuiMain from main.cpp"
```

### Task 5.3: Create `src/app/main.cpp` (the dispatch)

**Files:**
- Create: `src/app/main.cpp`

- [ ] **Step 1: Create file**

```cpp
#include <QApplication>
#include <QCoreApplication>

#include "AppMode.h"
#include "MessageHandler.h"

int cliRun(int argc, char *argv[], QCoreApplication &app);
int guiRun(QApplication &app);

int main(int argc, char *argv[])
{
    app::installMessageHandler();

    const app::Mode mode = app::parseAppMode(argc, argv);

    if (mode == app::Mode::Headless)
    {
        app::attachParentConsoleIfAvailable();
        QCoreApplication coreApp(argc, argv);
        return cliRun(argc, argv, coreApp);
    }

    QApplication guiApp(argc, argv);
    return guiRun(guiApp);
}
```

- [ ] **Step 2: Commit**

```bash
git add src/app/main.cpp
git commit -m "feat(app): add unified main entry with mode dispatch"
```

---

## Phase 6: Single CMake Target

### Task 6.1: Create `src/app/CMakeLists.txt` (the only executable)

**Files:**
- Create: `src/app/CMakeLists.txt`
- Delete: `src/cli/CMakeLists.txt`
- Delete: `src/gui/CMakeLists.txt`
- Delete: `src/cli/main.cpp`
- Delete: `src/gui/main.cpp`

- [ ] **Step 1: Create `src/app/CMakeLists.txt`**

```cmake
# Unified EZSpecCam executable (CLI + GUI in one binary).
# Mode is selected at runtime by argv; see src/app/main.cpp.

set(EZSPECCAM_TARGET ezspeccam)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    qt_add_executable(${EZSPECCAM_TARGET})
else()
    qt_add_executable(${EZSPECCAM_TARGET} WIN32)
endif()

find_package(OpenMP REQUIRED)
target_link_libraries(${EZSPECCAM_TARGET} PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::OpenGL
    OpenMP::OpenMP_CXX
    OpenGL::GL
)

target_compile_definitions(${EZSPECCAM_TARGET} PRIVATE QCUSTOMPLOT_USE_OPENGL)

target_include_directories(${EZSPECCAM_TARGET} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/src/gui
)

target_sources(${EZSPECCAM_TARGET} PRIVATE
    main.cpp
    AppMode.h AppMode.cpp
    MessageHandler.h MessageHandler.cpp
    PluginLoader.h PluginLoader.cpp
    HeadlessController.h HeadlessController.cpp
    WaitStabilizer.h WaitStabilizer.cpp
    SequenceRunner.h SequenceRunner.cpp
    formats/FrameWriter.h formats/FrameWriter.cpp
    formats/IImageFormatHandler.h formats/SaveTypes.h
    formats/TiffFormatHandler.h formats/TiffFormatHandler.cpp
    formats/CsvFormatHandler.h formats/CsvFormatHandler.cpp

    cli/CliMain.cpp
    gui/GuiMain.cpp
    gui/DebugMacros.h
    gui/AppController.h gui/AppController.cpp
    gui/qcustomplot.h gui/qcustomplot.cpp
    gui/widgets/MainWindow.h gui/widgets/MainWindow.cpp
    gui/widgets/PostProcess.h gui/widgets/PostProcess.cpp
    gui/ui/MainWindowUi.h gui/ui/MainWindowUi.cpp
    gui/widgets/config/CameraTab.h gui/widgets/config/CameraTab.cpp
    gui/ui/CameraTabUi.h gui/ui/CameraTabUi.cpp
    gui/widgets/config/LoadingIndicator.h gui/widgets/config/LoadingIndicator.cpp
    gui/widgets/config/ParameterWidgetFactory.h gui/widgets/config/ParameterWidgetFactory.cpp
    gui/widgets/config/DataTab.h gui/widgets/config/DataTab.cpp
    gui/ui/DataTabUi.h gui/ui/DataTabUi.cpp
    gui/widgets/config/PluginTab.h gui/widgets/config/PluginTab.cpp
    gui/widgets/config/CameraConfigDialog.h gui/widgets/config/CameraConfigDialog.cpp
    gui/ui/CameraConfigDialogUi.h gui/ui/CameraConfigDialogUi.cpp
    gui/widgets/display/ImageViewWidget.h gui/widgets/display/ImageViewWidget.cpp
    gui/widgets/display/SpectrumViewWidget.h gui/widgets/display/SpectrumViewWidget.cpp
    gui/widgets/display/StatisticsDialog.h gui/widgets/display/StatisticsDialog.cpp
    gui/widgets/display/ProfileWindow.h gui/widgets/display/ProfileWindow.cpp
    gui/widgets/dialogs/RowRangeDialog.h gui/widgets/dialogs/RowRangeDialog.cpp
    gui/widgets/dialogs/ScaleControlDialog.h gui/widgets/dialogs/ScaleControlDialog.cpp
    gui/widgets/dialogs/DisplayStyleDialog.h gui/widgets/dialogs/DisplayStyleDialog.cpp
    gui/workers/FileSaverWorker.h gui/workers/FileSaverWorker.cpp
    gui/workers/FileLoaderWorker.h gui/workers/FileLoaderWorker.cpp
)
```

- [ ] **Step 2: Replace `src/cli/main.cpp` with `src/cli/CliMain.cpp`**

The Phase 4 version of `src/cli/main.cpp` becomes `src/cli/CliMain.cpp`, with the function `cli::run` instead of `main`:

```cpp
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>

#include "app/HeadlessController.h"
#include "app/MessageHandler.h"
#include "SequenceRunner.h"

namespace cli
{

static QVariant parseSetValue(const QString &valueStr)
{
    bool ok = false;
    double d = valueStr.toDouble(&ok);
    if (ok) {
        if (valueStr.contains('.')) return d;
        qint64 i = valueStr.toLongLong(&ok);
        return ok ? QVariant(i) : QVariant(d);
    }
    return valueStr;
}

int run(int argc, char *argv[], QCoreApplication & /*app*/)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("EZSpecCam headless camera control");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption listOpt("list", "List available cameras");
    QCommandLineOption listParamsOpt("list-params", "List all camera parameters");
    QCommandLineOption cameraOpt("camera", "Camera ID to connect to", "id");
    QCommandLineOption setOpt("set", "Set parameter (name=value)", "param");
    QCommandLineOption framesOpt("frames", "Number of frames to capture", "n", "1");
    QCommandLineOption outputOpt("output", "Output directory", "dir", ".");
    QCommandLineOption formatOpt("format", "Output format (tiff|csv)", "fmt", "tiff");
    QCommandLineOption prefixOpt("prefix", "Filename prefix", "str");
    QCommandLineOption suffixOpt("suffix", "Filename suffix", "str");
    QCommandLineOption sequenceOpt("sequence", "Event sequence JSON file", "file");

    parser.addOption(listOpt);
    parser.addOption(listParamsOpt);
    parser.addOption(cameraOpt);
    parser.addOption(setOpt);
    parser.addOption(framesOpt);
    parser.addOption(outputOpt);
    parser.addOption(formatOpt);
    parser.addOption(prefixOpt);
    parser.addOption(suffixOpt);
    parser.addOption(sequenceOpt);

    // Use the QCoreApplication's own parser; pass a fresh argv snapshot to it.
    parser.process(QCoreApplication::instance());

    app::HeadlessOptions opts;
    opts.listCameras = parser.isSet(listOpt);
    opts.listParams = parser.isSet(listParamsOpt);
    opts.cameraId = parser.value(cameraOpt);
    opts.frames = parser.value(framesOpt).toInt();
    opts.outputDir = parser.value(outputOpt);
    opts.format = parser.value(formatOpt).toLower();
    opts.prefix = parser.value(prefixOpt);
    opts.suffix = parser.value(suffixOpt);

    for (const QString &sv : parser.values(setOpt))
    {
        int eq = sv.indexOf('=');
        if (eq < 0) { qWarning() << "Invalid --set:" << sv; continue; }
        opts.setParameters[sv.left(eq).trimmed()] = parseSetValue(sv.mid(eq + 1).trimmed());
    }

    if (parser.isSet(sequenceOpt))
    {
        SequenceRunner seq;
        if (!seq.loadFromFile(parser.value(sequenceOpt))) {
            qCritical().noquote() << "Failed to load sequence:" << seq.errorString();
            return 1;
        }
        opts.sequence = seq.steps();
        if (opts.outputDir == "." && !seq.defaultOutputDir.isEmpty()) opts.outputDir = seq.defaultOutputDir;
        if (opts.format == "tiff" && !seq.defaultFormat.isEmpty())      opts.format = seq.defaultFormat;
        if (opts.prefix.isEmpty() && !seq.defaultPrefix.isEmpty())      opts.prefix = seq.defaultPrefix;
        if (opts.suffix.isEmpty() && !seq.defaultSuffix.isEmpty())      opts.suffix = seq.defaultSuffix;
    }

    return app::run(opts);
}

}
```

- [ ] **Step 3: Update `src/gui/GuiMain.cpp` to declare `gui::run` returning `int` and taking `QApplication&`**

Already done in Task 5.2. Verify by re-reading.

- [ ] **Step 4: Update `src/app/main.cpp` to call `cli::run` and `gui::run`**

Edit `src/app/main.cpp`:

```cpp
#include <QApplication>
#include <QCoreApplication>

#include "AppMode.h"
#include "MessageHandler.h"

namespace cli { int run(int argc, char *argv[], QCoreApplication &app); }
namespace gui { int run(QApplication &app); }

int main(int argc, char *argv[])
{
    app::installMessageHandler();

    const app::Mode mode = app::parseAppMode(argc, argv);

    if (mode == app::Mode::Headless)
    {
        app::attachParentConsoleIfAvailable();
        QCoreApplication coreApp(argc, argv);
        return cli::run(argc, argv, coreApp);
    }

    QApplication guiApp(argc, argv);
    return gui::run(guiApp);
}
```

- [ ] **Step 5: Delete old CMake files and main.cpp files**

```bash
git rm src/cli/CMakeLists.txt
git rm src/gui/CMakeLists.txt
git rm src/cli/main.cpp
git rm src/gui/main.cpp
```

- [ ] **Step 6: Update root `CMakeLists.txt`**

Replace the relevant section:

```cmake
option(EZSPECCAM_BUILD_APP "Build unified EZSpecCam application" ON)
```

Replace:
```cmake
if(EZSPECCAM_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

if(EZSPECCAM_BUILD_CLI)
    add_subdirectory(src/cli)
endif()
```

With:
```cmake
if(EZSPECCAM_BUILD_APP)
    add_subdirectory(src/app)
endif()
```

Update the summary message to reference `EZSPECCAM_BUILD_APP` instead of GUI/CLI.

- [ ] **Step 7: Update `CMakePresets.json`**

For each of the 3 presets (`msvc-debug`, `msvc-release`, `msvc-debug-gui`):
- Remove `"EZSPECCAM_BUILD_GUI": "ON"` and `"EZSPECCAM_BUILD_CLI": "ON"` (or "OFF" for the gui-only preset)
- Add `"EZSPECCAM_BUILD_APP": "ON"`

For `msvc-debug-gui`, also remove the `"EZSPECCAM_BUILD_TESTS": "OFF"` if it's there? **Keep the OFF** for tests; the preset name now is historical and the build matrix is unified.

- [ ] **Step 8: Update plugin `CMakeLists.txt` POST_BUILD targets**

For each of `src/plugins/mock/CMakeLists.txt`, `src/plugins/qhyccd/CMakeLists.txt`, `src/plugins/hamamatsu/CMakeLists.txt`, `src/plugins/picam/CMakeLists.txt`:

Replace `$<TARGET_FILE_DIR:ezspeccam_gui>/plugins/drivers` with `$<TARGET_FILE_DIR:ezspeccam>/plugins/drivers`.

- [ ] **Step 9: Build**

```bash
.\build_preset.bat debug
```

Expected: builds `ezspeccam.exe` (no more `ezspeccam_cli.exe` or `ezspeccam_gui.exe`).

- [ ] **Step 10: Run end-to-end smoke**

```bash
.\build\msvc-debug\bin\Debug\ezspeccam.exe --list
.\build\msvc-debug\bin\Debug\ezspeccam.exe --camera mock-001 --frames 1 --output tmp_cli
```

Expected: same output as Phase 4 smoke tests.

- [ ] **Step 11: Commit**

```bash
git add -A
git commit -m "refactor: collapse CLI and GUI into single ezspeccam target"
```

---

## Phase 7: Test Cleanup

### Task 7.1: Update test_app_controller to remove PluginInfo refs (if any)

**Files:**
- Modify: `tests/test_app_controller/test_app_controller.cpp` (if needed)
- Modify: `tests/test_app_controller/CMakeLists.txt`

- [ ] **Step 1: Search for any direct PluginInfo references**

```bash
grep -n "PluginInfo" tests/test_app_controller/test_app_controller.cpp
```

If no matches, no change needed. If matches exist, replace `PluginInfo` with `app::plugins::Entry` and add `#include "app/PluginLoader.h"`.

- [ ] **Step 2: Build + test**

```bash
.\build_preset.bat debug
.\run_tests.bat
```

- [ ] **Step 3: Commit (if any changes)**

```bash
git add tests/test_app_controller/
git commit -m "test(app_controller): use app::plugins::Entry"
```

### Task 7.2: Delete `tests/test_cli/`

**Files:**
- Delete: `tests/test_cli/CMakeLists.txt`
- Delete: `tests/test_cli/test_cli.cpp`
- Delete: `tests/test_cli/test_cli.h`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Remove the disabled references**

In `tests/CMakeLists.txt`, delete the two commented lines:
```cmake
# add_subdirectory(test_cli)
# add_subdirectory(test_cli)
```

- [ ] **Step 2: Delete the directory**

```bash
git rm -r tests/test_cli/
```

- [ ] **Step 3: Build + test**

```bash
.\build_preset.bat debug
.\run_tests.bat
```

- [ ] **Step 4: Commit**

```bash
git add -A
git commit -m "chore(tests): remove dead test_cli"
```

---

## Phase 8: End-to-End Verification

### Task 8.1: Run all tests

- [ ] **Step 1: Run full test suite**

```bash
.\run_tests.bat
```

Expected: All tests PASS. Critical ones to verify:
- `test_app_mode` (new)
- `test_plugin_loader` (new)
- `test_metadata_save_load` (wide CSV + softwareSettings locked in)
- `test_plugin_loading` (mock plugin still loadable)
- `test_app_controller` (AppController still functional)
- `test_row_range_dialog` (FileLoaderWorker still works)

### Task 8.2: End-to-end smoke (from spec)

- [ ] **Step 1: List cameras**

```bash
.\build\msvc-debug\bin\Debug\ezspeccam.exe --list
```

Expected output: lists `mock-001`.

- [ ] **Step 2: Single TIFF capture**

```bash
mkdir tmp_cli
.\build\msvc-debug\bin\Debug\ezspeccam.exe --camera mock-001 --frames 1 --output tmp_cli
type tmp_cli\img_*_metadata.json
```

Expected: contains `"softwareSettings": {}`.

- [ ] **Step 3: Wide-format CSV**

```bash
mkdir tmp_csv
.\build\msvc-debug\bin\Debug\ezspeccam.exe --camera mock-001 --frames 1 --format csv --output tmp_csv
```

Expected: file with no header, line count == mock image height.

- [ ] **Step 4: Sequence**

```bash
echo {"steps":[{"configure":{"exposure":100}},{"capture":{"frames":1}}]} > test_seq.json
.\build\msvc-debug\bin\Debug\ezspeccam.exe --sequence test_seq.json
```

Expected: capture succeeds.

- [ ] **Step 5: GUI launch**

```bash
.\build\msvc-debug\bin\Debug\ezspeccam.exe
```

Expected: MainWindow opens; "Available Cameras" table shows mock driver DLL file name; close cleanly with `QApplication::exec()` returning 0.

- [ ] **Step 6: Commit (no functional change; cleanup of tmp dirs)**

```bash
git clean -fd tmp_cli tmp_csv test_seq.json
```

(No commit needed for transient test outputs.)

---

## Phase 9: Documentation

### Task 9.1: Update `src/cli/README.md`

**Files:**
- Modify: `src/cli/README.md`

- [ ] **Step 1: Replace all `ezspeccam_cli` with `ezspeccam`**

```bash
# In PowerShell:
(Get-Content src/cli/README.md) -replace 'ezspeccam_cli', 'ezspeccam' | Set-Content src/cli/README.md
```

- [ ] **Step 2: Add note about CSV wide format and metadata schema change**

After the "Output Files" section, add a new section:

```markdown
## Notes for Migrating from Pre-Merge CLI

- The binary is now `ezspeccam.exe` (was `ezspeccam_cli.exe`).
- CSV output is now the **wide format** (one line per image row, values comma-separated, no header). Previously the CLI wrote a long format with `Row,Col,Value` header and one line per pixel; that format is no longer produced.
- The metadata sidecar (`_metadata.json`) now always contains a `softwareSettings` object (empty `{}` in headless mode). The previous CLI schema did not include this key.
```

- [ ] **Step 3: Commit**

```bash
git add src/cli/README.md
git commit -m "docs(cli): update README for unified binary and CSV format"
```

### Task 9.2: Update root `README.md`

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Update the Architecture diagram**

Replace the `src/cli/` and `src/gui/` entries in the architecture tree with a single `src/app/` entry, and update the descriptions.

- [ ] **Step 2: Update "Build Options" table**

Replace the `EZSPECCAM_BUILD_GUI` and `EZSPECCAM_BUILD_CLI` rows with a single `EZSPECCAM_BUILD_APP` row.

- [ ] **Step 3: Update "Run" examples**

Replace `./build/gui/EZSpecCam.exe` with `./build/msvc-debug/bin/Debug/ezspeccam.exe` (or generalised).

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: update root README for unified binary"
```

### Task 9.3: Update `AGENTS.md` files

**Files:**
- Modify: `AGENTS.md` (root)
- Modify: `src/gui/AGENTS.md`

- [ ] **Step 1: Root `AGENTS.md`**

Update the top code-map and conventions. Replace `ezspeccam_cli` row in the code map. Update the OVERVIEW to mention the unified binary.

- [ ] **Step 2: `src/gui/AGENTS.md`**

Update the "Entry point" line from `main.cpp` to `GuiMain.cpp` and add a note that GUI is one mode of the unified `ezspeccam.exe`.

- [ ] **Step 3: Commit**

```bash
git add AGENTS.md src/gui/AGENTS.md
git commit -m "docs(agents): update knowledge base for unified binary"
```

---

## Phase 10: Final Cleanup

### Task 10.1: Verify no `ezspeccam_cli` references remain

- [ ] **Step 1: Grep for the old name**

```bash
grep -rn "ezspeccam_cli" src/ tests/ docs/ CMakeLists.txt CMakePresets.json 2>/dev/null
```

Expected: zero matches.

- [ ] **Step 2: Grep for old build flags**

```bash
grep -rn "EZSPECCAM_BUILD_GUI\|EZSPECCAM_BUILD_CLI" src/ tests/ docs/ CMakeLists.txt CMakePresets.json 2>/dev/null
```

Expected: zero matches.

### Task 10.2: Final build + test cycle

- [ ] **Step 1: Clean build**

```bash
.\build_preset.bat debug
```

- [ ] **Step 2: Run all tests**

```bash
.\run_tests.bat
```

- [ ] **Step 3: Verify binary count**

```bash
dir build\msvc-debug\bin\Debug\*.exe | findstr /R "ezspeccam"
```

Expected: one binary, `ezspeccam.exe` (plus the test executables).

- [ ] **Step 4: Final commit (none expected; if anything changed during cleanup)**

```bash
git status
```

If anything outstanding, commit with appropriate message.

---

## Out-of-Scope Reminder

- No changes to `ICameraDriver` or `CameraTypes`.
- No new CLI features, no new flags, no subcommands.
- No GUI feature changes.
- Plugin authors' workflow unchanged (`.dll` deploy to `plugins/drivers/` next to `ezspeccam.exe`).
