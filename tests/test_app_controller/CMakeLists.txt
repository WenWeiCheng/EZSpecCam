# Test AppController
add_executable(test_app_controller)
target_sources(test_app_controller PRIVATE
    test_app_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/mock/MockCameraDriver.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/AppController.cpp
    ${CMAKE_SOURCE_DIR}/src/app/PluginLoader.cpp
)
target_compile_definitions(test_app_controller PRIVATE EZSPECCAM_MOCK_TESTING)
target_link_libraries(test_app_controller PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_app_controller PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/app
    ${CMAKE_SOURCE_DIR}/src/plugins/mock
    ${CMAKE_SOURCE_DIR}/src/gui
)
set_target_properties(test_app_controller PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_app_controller COMMAND test_app_controller)