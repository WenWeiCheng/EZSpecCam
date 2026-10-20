# tests — Test Suite

**Generated:** 2026-05-17

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
├── test_qhyccd_driver/             # QHYCCD driver tests
├── test_dark_calibration/          # Legacy PostProcess dark-subtraction tests
├── test_flat_calibration/          # Calibration math: dark (both methods) + flat (both methods, dark-model reuse)
├── test_calibration_perf/          # Calibration apply/build benchmarks (prints timings, no thresholds)
├── test_despike/                   # PostProcess::despikeImage algorithm + row-range tests
├── test_despike_dialog/            # DespikeDialog single-parameter behaviour
├── test_despike_pipeline/          # MainWindow despike → vbin → display pipeline + row range
└── test_view_widgets_perf/         # View widget performance tests
```

## WHERE TO LOOK
| Task | Location |
|------|----------|
| Adding a new test | Copy existing test dir → new CMakeLists.txt → add to `tests/CMakeLists.txt` |
| Mock driver integration test | `test_mock_driver/test_mock_driver.cpp` |
| Plugin loading test | `test_plugin_loading/` |
| Image despike algorithm + row range | `test_despike/test_despike.cpp` |
| Despike parameters dialog | `test_despike_dialog/test_despike_dialog.cpp` |
| Despike pipeline (spectrum from despiked data, row range, save state) | `test_despike_pipeline/test_despike_pipeline.cpp` |
| Despike benchmarks (6000×4000 full image / 100 rows, prints FPS) | `test_postprocess_perf/test_postprocess_perf.cpp` |
| Performance benchmarks | `test_view_widgets_perf/` |

## CONVENTIONS
- Framework: **Qt Test** (not Google Test / Catch2)
- Each test class inherits `QObject` and uses `Q_OBJECT`
- Test slots: `initTestCase()`, `cleanupTestCase()`, `init()`, `cleanup()`, test methods
- Assertions: `QVERIFY2()`, `QCOMPARE()`, `QVERIFY()`
- Disabled tests (`test_cli`) are commented out in parent `CMakeLists.txt`
- when test signals, should use `QSignalSpy`, and use `spy.wait()` to wait for the signal and check `spy.count()` to check the signal was emitted. Both ways should use together incase of signals arrive before `wait()`.
- test methods should be named `test_<feature>()`
- About driver tests, should cover: enumerate, connect, signal, capture(single/live/burst) parameter(one parameter, one test method, this may need know certain camera's supported parameters). Refer to `test_qhyccd_driver/test_qhyccd_driver.cpp`

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
