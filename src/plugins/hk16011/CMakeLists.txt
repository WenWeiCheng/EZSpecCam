#==============================================================================
# HK16011 CCD camera driver plugin
#==============================================================================
#
# Links the vendor SDK through the CMake package it installs
# (<prefix>/lib/cmake/HK16011). The SDK is a single shared library that
# already contains libusb-1.0 and libserialport, so the plugin links one
# imported target and has exactly one runtime library to deploy beside it.
#
# Point HK16011_ROOT at the install prefix (e.g. C:/sdk/hk16011); CMake's own
# -DHK16011_DIR=<prefix>/lib/cmake/HK16011 and CMAKE_PREFIX_PATH work as well.
#
# When the SDK is missing the plugin is skipped with a WARNING so the rest of
# the project still builds; set EZSPECCAM_REQUIRE_HK16011=ON to make it fatal.

option(EZSPECCAM_REQUIRE_HK16011
       "Fail the build if the HK16011 SDK is not found (default: skip with warning)" OFF)

# —— Locate the SDK ——

set(HK16011_ROOT "$ENV{HK16011_ROOT}" CACHE PATH "HK16011 SDK install prefix")

find_package(HK16011 CONFIG QUIET HINTS "${HK16011_ROOT}")

if(NOT HK16011_FOUND)
    if(EZSPECCAM_REQUIRE_HK16011)
        message(FATAL_ERROR
            "HK16011 SDK not found. Install 04-driver to a prefix and point "
            "HK16011_ROOT at it, e.g.\n"
            "  cmake --install 04-driver/build --config Release --prefix C:/sdk/hk16011\n"
            "  set HK16011_ROOT=C:/sdk/hk16011")
    endif()
    message(WARNING
        "HK16011 SDK not found. Skipping the hk16011 plugin. "
        "Set HK16011_ROOT to the SDK install prefix or pass "
        "-DHK16011_DIR=<prefix>/lib/cmake/HK16011. "
        "Pass -DEZSPECCAM_REQUIRE_HK16011=ON to upgrade this to an error.")
    return()
endif()

message(STATUS "HK16011 SDK: ${HK16011_INCLUDE_DIR}")

# —— Plugin target ——

qt_add_plugin(hk16011_camera_driver MODULE)

# HK16011::HK16011 carries the include directory and the HK16011_SHARED
# compile definition the public header needs on Windows (dllimport).
target_link_libraries(hk16011_camera_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Widgets
    HK16011::HK16011
)

target_include_directories(hk16011_camera_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_sources(hk16011_camera_driver PRIVATE
    Hk16011Driver.h Hk16011Driver.cpp
    hk16011.json
)

# —— Post-build deploy ——

# Only the plugin module goes into plugins/drivers/: PluginLoader tries to load
# every file in that directory as a plugin, so copying the SDK's runtime library
# there would show up as a plugin load failure (test_app_controller catches it).
add_custom_command(TARGET hk16011_camera_driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ezspeccam>/plugins/drivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:hk16011_camera_driver>"
        "$<TARGET_FILE_DIR:ezspeccam>/plugins/drivers/"
    COMMENT "Deploying hk16011_camera_driver to plugins/drivers/"
)

if(WIN32)
    # Windows resolves a plugin's dependencies through the *executable's*
    # directory, so HK16011.dll belongs next to ezspeccam.exe.
    add_custom_command(TARGET hk16011_camera_driver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:HK16011::HK16011>"
            "$<TARGET_FILE_DIR:ezspeccam>"
        COMMENT "Deploying HK16011.dll next to ezspeccam"
    )
else()
    # ELF: find libHK16011.so through RUNPATH instead of shipping a copy.
    set_target_properties(hk16011_camera_driver PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:HK16011::HK16011>;$ORIGIN"
        INSTALL_RPATH "$ORIGIN"
    )
endif()
