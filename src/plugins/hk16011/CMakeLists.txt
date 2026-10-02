#==============================================================================
# HK16011 CCD camera driver plugin
#==============================================================================
#
# Wraps the vendor C SDK (libHK16011), which is Linux-only and not installed
# by the vendor. Point HK16011_ROOT at the SDK source tree, or override it per
# build with -DHK16011_SDK_ROOT=<path> or a CMakeUserPresets.json entry.
#
# Expected layout:
#   ${HK16011_ROOT}/
#   ├── include/hk16011.h
#   └── lib/libHK16011.so
#
# When the SDK is missing the plugin is skipped with a WARNING so the rest of
# the project still builds; set EZSPECCAM_REQUIRE_HK16011=ON to make it fatal.

option(EZSPECCAM_REQUIRE_HK16011
       "Fail the build if the HK16011 SDK is not found (default: skip with warning)" OFF)

# —— Locate the SDK ——
#
# Priority: explicit -DHK16011_SDK_ROOT=... > HK16011_ROOT environment variable

set(HK16011_SDK_ROOT "" CACHE PATH "Override HK16011 SDK path (default: $ENV{HK16011_ROOT})")

if(HK16011_SDK_ROOT)
    set(_hk_root "${HK16011_SDK_ROOT}")
elseif(DEFINED ENV{HK16011_ROOT})
    set(_hk_root "$ENV{HK16011_ROOT}")
    set(HK16011_SDK_ROOT "${_hk_root}" CACHE PATH "HK16011 SDK path" FORCE)
endif()

if(NOT _hk_root
        OR NOT EXISTS "${_hk_root}/include/hk16011.h"
        OR NOT EXISTS "${_hk_root}/lib/libHK16011.so")
    if(EZSPECCAM_REQUIRE_HK16011)
        message(FATAL_ERROR
            "HK16011 SDK not found. Set HK16011_ROOT to the SDK tree "
            "(containing include/hk16011.h and lib/libHK16011.so).")
    else()
        message(WARNING
            "HK16011 SDK not found. Skipping hk16011 plugin. "
            "Set HK16011_ROOT to enable it.")
        return()
    endif()
endif()

set(HK16011_INCLUDE_DIR "${_hk_root}/include")
set(HK16011_LIBRARY_DIR "${_hk_root}/lib")
message(STATUS "HK16011 SDK: ${_hk_root}")

# —— Plugin target ——

qt_add_plugin(hk16011_camera_driver MODULE)

target_link_libraries(hk16011_camera_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Widgets
    "${HK16011_LIBRARY_DIR}/libHK16011.so"
)

target_include_directories(hk16011_camera_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    "${HK16011_INCLUDE_DIR}"
)

target_sources(hk16011_camera_driver PRIVATE
    Hk16011Driver.h Hk16011Driver.cpp
    hk16011.json
)

# Resolve libHK16011.so.1 next to the plugin once it has been deployed there.
set_target_properties(hk16011_camera_driver PROPERTIES
    BUILD_RPATH "${HK16011_LIBRARY_DIR};$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

# —— Post-build deploy ——

# Plugin module -> plugins/drivers/
add_custom_command(TARGET hk16011_camera_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ezspeccam>/plugins/drivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:hk16011_camera_driver>"
        "$<TARGET_FILE_DIR:ezspeccam>/plugins/drivers/"
    COMMENT "Deploying hk16011_camera_driver to plugins/drivers/"
)

# The SDK library -> plugins/drivers/, under the SONAME the plugin records as
# its DT_NEEDED, so the deployed plugin resolves it without touching LD_LIBRARY_PATH.
add_custom_command(TARGET hk16011_camera_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${HK16011_LIBRARY_DIR}/libHK16011.so.1.0.0"
        "$<TARGET_FILE_DIR:ezspeccam>/plugins/drivers/libHK16011.so.1"
    COMMENT "Deploying libHK16011.so.1 to plugins/drivers/"
)
