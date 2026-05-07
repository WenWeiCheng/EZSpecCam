# Test plugin loading
add_executable(test_plugin_loading)
target_sources(test_plugin_loading PRIVATE
    test_plugin_loading.cpp
)
add_dependencies(test_plugin_loading mock_camera_driver)

target_link_libraries(test_plugin_loading PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_plugin_loading PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
# Pass the full path to the compiled mock_camera_driver plugin DLL at compile time
target_compile_definitions(test_plugin_loading PRIVATE
    MOCK_PLUGIN_PATH="$<TARGET_FILE:mock_camera_driver>"
)
set_target_properties(test_plugin_loading PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_plugin_loading COMMAND test_plugin_loading)