# Dark-frame (bias + scaled) and flat-field (classic / variable exposure)
# calibration math. Pure functions, no camera or MainWindow required.
add_executable(test_flat_calibration
    test_flat_calibration.cpp
)
target_link_libraries(test_flat_calibration PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_flat_calibration PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
)
add_test(NAME test_flat_calibration COMMAND test_flat_calibration)
