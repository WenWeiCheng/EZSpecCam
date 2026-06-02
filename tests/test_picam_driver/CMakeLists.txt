# Test PICam driver (PIXIS100B model)
#
# By default, tests run against the PICam SDK internal demo camera (Pixis100B).
# To test against a real physical camera, build with:
#   -DUSE_REAL_CAMERA=ON
#
# Or define USE_REAL_CAMERA in your build system.

# Locate PICam SDK (same logic as src/plugins/picam/CMakeLists.txt)
set(PICAM_SDK_ROOT "" CACHE PATH "Override PICam SDK path (default: $ENV{PicamRoot})")

if(PICAM_SDK_ROOT)
    set(_picam_root "${PICAM_SDK_ROOT}")
elseif(DEFINED ENV{PicamRoot})
    set(_picam_root "$ENV{PicamRoot}")
else()
    message(FATAL_ERROR "PICam SDK not found. Set PicamRoot environment variable or pass -DPICAM_SDK_ROOT=<path>")
endif()

set(PICAM_INCLUDE_DIR "${_picam_root}/Includes")
set(PICAM_LIBRARY_DIR "${_picam_root}/Libraries")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PICAM_LIB_NAME "picam.lib")
else()
    set(PICAM_LIB_NAME "picam32.lib")
endif()

message(STATUS "PICam SDK for tests: ${_picam_root}")

# Create the test executable
add_executable(test_picam_driver_PIXIS100B)

target_sources(test_picam_driver_PIXIS100B PRIVATE
    test_picam_driver_PIXIS100B.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/picam/PicamDriver.cpp
)

target_link_libraries(test_picam_driver_PIXIS100B PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
    "${PICAM_LIBRARY_DIR}/${PICAM_LIB_NAME}"
)

target_include_directories(test_picam_driver_PIXIS100B PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/picam
    "${PICAM_INCLUDE_DIR}"
)

# Option to test against real camera instead of demo
option(USE_REAL_CAMERA "Test against real PICam camera instead of demo" OFF)
if(USE_REAL_CAMERA)
    target_compile_definitions(test_picam_driver_PIXIS100B PRIVATE USE_REAL_CAMERA)
endif()

set_target_properties(test_picam_driver_PIXIS100B PROPERTIES
    Qt6::MinimalQt6 TRUE
)

# add_test(NAME test_picam_driver_PIXIS100B COMMAND test_picam_driver_PIXIS100B)
