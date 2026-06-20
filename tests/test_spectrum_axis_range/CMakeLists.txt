# Axis range behaviour tests for SpectrumViewWidget
add_executable(test_spectrum_axis_range)
target_sources(test_spectrum_axis_range PRIVATE
    test_spectrum_axis_range.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/SpectrumViewWidget.cpp
)
target_link_libraries(test_spectrum_axis_range PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
target_compile_definitions(test_spectrum_axis_range PRIVATE
    QCUSTOMPLOT_USE_OPENGL
    SPECTRUM_AXIS_RANGE_TESTING
)
target_include_directories(test_spectrum_axis_range PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
set_target_properties(test_spectrum_axis_range PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_spectrum_axis_range COMMAND test_spectrum_axis_range)