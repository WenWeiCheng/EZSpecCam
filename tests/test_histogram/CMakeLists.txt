# Histogram window behaviour: binning, custom overexposure threshold and the
# log-axis path (QCPBars cannot draw a bar whose base is 0 on a log axis).
add_executable(test_histogram)
target_sources(test_histogram PRIVATE
    test_histogram.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/HistogramViewWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/HistogramWindow.cpp
)
target_link_libraries(test_histogram PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
target_compile_definitions(test_histogram PRIVATE
    QCUSTOMPLOT_USE_OPENGL
)
target_include_directories(test_histogram PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
set_target_properties(test_histogram PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_histogram COMMAND test_histogram)
