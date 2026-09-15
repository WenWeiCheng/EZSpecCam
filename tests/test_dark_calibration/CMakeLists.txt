# Tests for PostProcess::applyDarkCalibration
add_executable(test_dark_calibration)
target_sources(test_dark_calibration PRIVATE
    test_dark_calibration.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/PostProcess.cpp
)
target_link_libraries(test_dark_calibration PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_dark_calibration PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gui/widgets
)
add_test(NAME test_dark_calibration COMMAND test_dark_calibration)
