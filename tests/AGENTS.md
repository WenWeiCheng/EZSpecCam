# tests — Test Suite

**Generated:** 2026-05-12

## OVERVIEW
Qt Test framework (`Qt6::Test`). Each test module is a separate executable with its own `CMakeLists.txt`. Run via `ctest`.

## STRUCTURE
```
tests/
├── CMakeLists.txt                  # Test suite coordinator
├── test_app_controller/            # AppController state machine tests
├── test_cli/                       # CLI app tests (DISABLED)
├── test_configuration/             # Camera configuration tests
├── test_mock_driver/               # MockCameraDriver tests (~820 lines)
├── test_plugin_loading/            # Plugin discovery + loading tests
├── test_profile_demo/              # Profile/stats widget tests
├── test_qhyccd_driver/             # QHYCCD driver tests (DISABLED — needs hardware)
└── test_view_widgets_perf/         # View widget performance tests
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Adding a new test | Copy existing test dir → new CMakeLists.txt → add to `tests/CMakeLists.txt` |
| Mock driver integration test | `test_mock_driver/test_mock_driver.cpp` |
| Plugin loading test | `test_plugin_loading/` |
| Performance benchmarks | `test_view_widgets_perf/` |

## CONVENTIONS
- Framework: **Qt Test** (not Google Test / Catch2)
- Each test class inherits `QObject` and uses `Q_OBJECT`
- Test slots: `initTestCase()`, `cleanupTestCase()`, `init()`, `cleanup()`, test methods
- Assertions: `QVERIFY2()`, `QCOMPARE()`, `QVERIFY()`
- Disabled tests (`test_cli`, `test_qhyccd_driver`) are commented out in parent `CMakeLists.txt`
- when test signals, you should use `QSignalSpy`, and use `spy.wait()` to wait for the signal and check `spy.count()` to check the signal was emitted. Both ways should use together incase of signals arrive before `wait()`.

## COMMANDS
```bash
# Run all tests (after build)
ctest --test-dir build/msvc-debug -C Debug --output-on-failure

# Run a single test
ctest --test-dir build/msvc-debug -C Debug -R test_mock_driver --output-on-failure
```

## ANTI-PATTERNS
- **DO NOT** assume camera hardware is present — use `MockCameraDriver` for portable tests
- **DO NOT** skip `Q_OBJECT` macro in test classes (MOC processing required)
