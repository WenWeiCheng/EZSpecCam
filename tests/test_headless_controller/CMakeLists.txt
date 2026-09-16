# Regression tests for HeadlessController::captureFrames
# - Verifies that two consecutive capture calls do not race on stale
#   captureStopped signal connections from the previous call.
set(CMAKE_AUTOMOC ON)
add_executable(test_headless_controller)
target_sources(test_headless_controller PRIVATE
    test_headless_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/HeadlessController.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/MessageHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/ParameterClamper.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/WaitStabilizer.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/SequenceRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/mock/MockCameraDriver.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/mock/MockCameraDriver.h
    ${CMAKE_SOURCE_DIR}/src/formats/FrameWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/formats/FrameWriter.h
    ${CMAKE_SOURCE_DIR}/src/formats/IImageFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/formats/SaveTypes.h
    ${CMAKE_SOURCE_DIR}/src/formats/TiffFormatHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/formats/TiffFormatHandler.h
    ${CMAKE_SOURCE_DIR}/src/formats/CsvFormatHandler.cpp
    ${CMAKE_SOURCE_DIR}/src/formats/CsvFormatHandler.h
)
target_compile_definitions(test_headless_controller PRIVATE EZSPECCAM_MOCK_TESTING)
target_link_libraries(test_headless_controller PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_headless_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/src/plugins/mock
)
add_test(NAME test_headless_controller COMMAND test_headless_controller)