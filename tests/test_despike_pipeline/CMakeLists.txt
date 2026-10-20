# Drives MainWindow's despike → vertical binning pipeline.
#
# Reuses the application's own GUI source list (minus its main.cpp) so this test
# exercises exactly the code the shipped binary compiles.
find_package(OpenMP REQUIRED)

set(TEST_DESPIKE_PIPELINE_SOURCES ${EZSPECCAM_GUI_SOURCES})
list(REMOVE_ITEM TEST_DESPIKE_PIPELINE_SOURCES ${CMAKE_SOURCE_DIR}/src/gui/main.cpp)
list(REMOVE_ITEM TEST_DESPIKE_PIPELINE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/ICameraDriver.h
    ${CMAKE_SOURCE_DIR}/src/core/CameraTypes.h
    ${CMAKE_SOURCE_DIR}/src/core/PluginLoader.h
)

add_executable(test_despike_pipeline
    test_despike_pipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ICameraDriver.h
    ${CMAKE_SOURCE_DIR}/src/core/CameraTypes.h
    ${CMAKE_SOURCE_DIR}/src/core/PluginLoader.h
    ${CMAKE_SOURCE_DIR}/src/core/PluginLoader.cpp
    ${TEST_DESPIKE_PIPELINE_SOURCES}
)

target_include_directories(test_despike_pipeline PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
)

target_compile_definitions(test_despike_pipeline PRIVATE QCUSTOMPLOT_USE_OPENGL)

target_link_libraries(test_despike_pipeline PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenMP::OpenMP_CXX
    OpenGL::GL
)

add_test(NAME test_despike_pipeline COMMAND test_despike_pipeline)
