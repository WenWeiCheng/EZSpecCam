# Sliding-window history buffer behind CameraTab's telemetry polling.
add_executable(test_parameter_history)
target_sources(test_parameter_history PRIVATE
    test_parameter_history.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/ParameterHistoryStore.cpp
)
target_link_libraries(test_parameter_history PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Test
)
target_include_directories(test_parameter_history PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config
)
set_target_properties(test_parameter_history PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_parameter_history COMMAND test_parameter_history)
