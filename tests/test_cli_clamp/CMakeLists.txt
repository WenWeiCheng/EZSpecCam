# CLI parameter clamp behaviour tests
add_executable(test_cli_clamp)
target_sources(test_cli_clamp PRIVATE
    test_cli_clamp.cpp
    ${CMAKE_SOURCE_DIR}/src/cli/ParameterClamper.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/mock/MockCameraDriver.cpp
)
target_compile_definitions(test_cli_clamp PRIVATE EZSPECCAM_MOCK_TESTING)
target_link_libraries(test_cli_clamp PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_cli_clamp PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/cli
    ${CMAKE_SOURCE_DIR}/src/plugins/mock
)
add_test(NAME test_cli_clamp COMMAND test_cli_clamp)