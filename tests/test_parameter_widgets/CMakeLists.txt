# Unit-scaled parameter widget: a parameter whose constraint carries a unit
# selector must render as a float range and keep what the user typed. The
# integer variant truncated the scaled display value (10 s came back as 9 s),
# which is why hk16011 presents exposure_time_us as a float range.
add_executable(test_parameter_widgets)
target_sources(test_parameter_widgets PRIVATE
    test_parameter_widgets.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/ParameterWidgetFactory.cpp
)
target_link_libraries(test_parameter_widgets PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_parameter_widgets PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config
)
set_target_properties(test_parameter_widgets PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_parameter_widgets COMMAND test_parameter_widgets)
