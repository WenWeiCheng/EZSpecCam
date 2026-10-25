#==============================================================================
# HK16011 driver test
#==============================================================================
#
# The driver source is compiled in directly (not loaded through QPluginLoader),
# so the test links the same HK16011::HK16011 package target the plugin does.
# find_package reuses the HK16011_DIR the plugin already cached, so the two
# cannot resolve different SDKs.
#
# The test skips with a WARNING when the SDK is absent, and skips itself at
# runtime when no camera is attached, so it is safe to run in CI.

find_package(HK16011 CONFIG QUIET)

if(NOT HK16011_FOUND)
    message(WARNING
        "HK16011 SDK not found. Skipping test_hk16011_driver_HK16011. "
        "Set HK16011_ROOT to the SDK install prefix.")
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
    HK16011::HK16011
)

target_include_directories(test_hk16011_driver_HK16011 PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/hk16011
)

set_target_properties(test_hk16011_driver_HK16011 PROPERTIES
    Qt6::MinimalQt6 TRUE
)

# The test executable sits in the same directory as ezspeccam, which is where
# the plugin's deploy step puts HK16011.dll on Windows (plugin dependencies are
# resolved through the executable's directory); copy it here too so the test can
# be built and run on its own. It must not go into plugins/drivers/, where
# PluginLoader would try to load it as a plugin.
if(WIN32)
    add_custom_command(TARGET test_hk16011_driver_HK16011 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:HK16011::HK16011>"
            "$<TARGET_FILE_DIR:test_hk16011_driver_HK16011>/"
    )
else()
    set_target_properties(test_hk16011_driver_HK16011 PROPERTIES
        BUILD_RPATH "$<TARGET_FILE_DIR:HK16011::HK16011>;$ORIGIN"
    )
endif()

add_test(NAME test_hk16011_driver_HK16011 COMMAND test_hk16011_driver_HK16011)
