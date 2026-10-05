cmake_minimum_required(VERSION 3.20)
project(test_calibration_dialog)

# The dark-frame acquisition controls live inside CalibrationDialog, so the
# widget's own behaviour (button locking, the in-memory marker) is worth
# covering without standing up a camera.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 COMPONENTS Core Gui Widgets Test REQUIRED)

add_executable(test_calibration_dialog
    test_calibration_dialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/workers/FileLoaderWorker.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/dialogs/CalibrationDialog.cpp
)

target_include_directories(test_calibration_dialog PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gui/workers
)

target_link_libraries(test_calibration_dialog PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)

add_test(NAME test_calibration_dialog COMMAND test_calibration_dialog)
