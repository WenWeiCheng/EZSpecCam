# Test mock driver
add_executable(test_mock_driver)
target_sources(test_mock_driver PRIVATE
    test_mock_driver.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/mock/MockCameraDriver.cpp
)
target_compile_definitions(test_mock_driver PRIVATE EZSPECCAM_MOCK_TESTING)
target_link_libraries(test_mock_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_mock_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/mock
)
set_target_properties(test_mock_driver PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_mock_driver COMMAND test_mock_driver)