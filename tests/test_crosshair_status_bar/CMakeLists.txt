# Guards the status-bar crosshair readout in MainWindow.
#
# Reuses the application's own GUI source list (minus its main.cpp) so this test
# exercises exactly the code the shipped binary compiles.
find_package(OpenMP REQUIRED)

set(TEST_CROSSHAIR_STATUS_BAR_SOURCES ${EZSPECCAM_GUI_SOURCES})
list(REMOVE_ITEM TEST_CROSSHAIR_STATUS_BAR_SOURCES ${CMAKE_SOURCE_DIR}/src/gui/main.cpp)
list(REMOVE_ITEM TEST_CROSSHAIR_STATUS_BAR_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/ICameraDriver.h
    ${CMAKE_SOURCE_DIR}/src/core/CameraTypes.h
    ${CMAKE_SOURCE_DIR}/src/core/PluginLoader.h
)

add_executable(test_crosshair_status_bar
    test_crosshair_status_bar.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ICameraDriver.h
    ${CMAKE_SOURCE_DIR}/src/core/CameraTypes.h
    ${CMAKE_SOURCE_DIR}/src/core/PluginLoader.h
    ${CMAKE_SOURCE_DIR}/src/core/PluginLoader.cpp
    ${TEST_CROSSHAIR_STATUS_BAR_SOURCES}
)

target_include_directories(test_crosshair_status_bar PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
)

target_compile_definitions(test_crosshair_status_bar PRIVATE QCUSTOMPLOT_USE_OPENGL)

target_link_libraries(test_crosshair_status_bar PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenMP::OpenMP_CXX
    OpenGL::GL
)

add_test(NAME test_crosshair_status_bar COMMAND test_crosshair_status_bar)
