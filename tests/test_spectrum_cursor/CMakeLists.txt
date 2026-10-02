# Cursor readout behaviour tests for SpectrumViewWidget: the readout must follow
# new data even when the mouse has not moved (live mode).
add_executable(test_spectrum_cursor)
target_sources(test_spectrum_cursor PRIVATE
    test_spectrum_cursor.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/SpectrumViewWidget.cpp
)
target_link_libraries(test_spectrum_cursor PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
target_compile_definitions(test_spectrum_cursor PRIVATE
    QCUSTOMPLOT_USE_OPENGL
)
target_include_directories(test_spectrum_cursor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
set_target_properties(test_spectrum_cursor PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_spectrum_cursor COMMAND test_spectrum_cursor)
