# Benchmarks for the calibration module. Like test_postprocess_perf, these
# print timings instead of enforcing thresholds.
add_executable(test_calibration_perf
    test_calibration_perf.cpp
)
target_link_libraries(test_calibration_perf PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_calibration_perf PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)
add_test(NAME test_calibration_perf COMMAND test_calibration_perf)
