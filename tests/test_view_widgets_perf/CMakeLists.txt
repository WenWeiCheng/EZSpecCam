# Performance tests for ImageViewWidget and SpectrumViewWidget
add_executable(test_view_widgets_perf)
target_sources(test_view_widgets_perf PRIVATE
    test_view_widgets_perf.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ImageViewWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/SpectrumViewWidget.cpp
)
target_link_libraries(test_view_widgets_perf PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
target_include_directories(test_view_widgets_perf PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
set_target_properties(test_view_widgets_perf PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_view_widgets_perf COMMAND test_view_widgets_perf)