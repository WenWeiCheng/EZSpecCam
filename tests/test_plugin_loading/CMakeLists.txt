# Test plugin loading
add_executable(test_plugin_loading)
target_sources(test_plugin_loading PRIVATE
    test_plugin_loading.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/mock/MockCameraDriver.cpp
)
target_link_libraries(test_plugin_loading PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_plugin_loading PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/mock
)
set_target_properties(test_plugin_loading PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_plugin_loading COMMAND test_plugin_loading)