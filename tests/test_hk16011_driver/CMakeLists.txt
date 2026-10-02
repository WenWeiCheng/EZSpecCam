#==============================================================================
# HK16011 driver test
#==============================================================================
#
# Mirrors the SDK probe from src/plugins/hk16011/CMakeLists.txt: the test needs
# the vendor header and library, so it skips with a WARNING when the SDK is
# absent rather than breaking the build.
#
# The test is registered with ctest. It skips itself at runtime when no camera
# is attached, so it is safe to run in CI.

set(HK16011_TEST_SDK_ROOT "" CACHE PATH
    "Override HK16011 SDK path (default: HK16011_SDK_ROOT, then \$ENV{HK16011_ROOT})")

# Reuse whatever src/plugins/hk16011 already resolved instead of re-deriving it:
# that subdirectory is added first, so HK16011_SDK_ROOT is settled by now. Without
# this the test silently drops out of ctest whenever the SDK was located through
# -D or a cache value left by an earlier configure rather than the environment —
# the plugin builds, the test does not exist, and nothing says so.
if(NOT HK16011_TEST_SDK_ROOT)
    if(HK16011_SDK_ROOT)
        set(HK16011_TEST_SDK_ROOT "${HK16011_SDK_ROOT}" CACHE PATH "HK16011 SDK path" FORCE)
    elseif(DEFINED ENV{HK16011_ROOT})
        set(HK16011_TEST_SDK_ROOT "$ENV{HK16011_ROOT}" CACHE PATH "HK16011 SDK path" FORCE)
    endif()
endif()

set(_hk_test_root "${HK16011_TEST_SDK_ROOT}")

if(NOT _hk_test_root
        OR NOT EXISTS "${_hk_test_root}/include/hk16011.h"
        OR NOT EXISTS "${_hk_test_root}/lib/libHK16011.so")
    message(WARNING
        "HK16011 SDK not found. Skipping test_hk16011_driver_HK16011. "
        "Set HK16011_ROOT to enable it.")
    return()
endif()

add_executable(test_hk16011_driver_HK16011)

target_sources(test_hk16011_driver_HK16011 PRIVATE
    test_hk16011_driver_HK16011.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/hk16011/Hk16011Driver.cpp
)

target_link_libraries(test_hk16011_driver_HK16011 PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
    "${_hk_test_root}/lib/libHK16011.so"
)

target_include_directories(test_hk16011_driver_HK16011 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/hk16011
    "${_hk_test_root}/include"
)

# The driver is compiled in directly rather than loaded as a plugin, so it
# needs the SDK at run time too.
set_target_properties(test_hk16011_driver_HK16011 PROPERTIES
    Qt6::MinimalQt6 TRUE
    BUILD_RPATH "${_hk_test_root}/lib"
)

add_test(NAME test_hk16011_driver_HK16011 COMMAND test_hk16011_driver_HK16011)
