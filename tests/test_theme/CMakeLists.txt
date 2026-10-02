# Light/dark theme behaviour tests for Theme and the plotting widgets
add_executable(test_theme)
target_sources(test_theme PRIVATE
    test_theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/SpectrumViewWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ImageViewWidget.cpp
)
target_link_libraries(test_theme PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
target_compile_definitions(test_theme PRIVATE
    QCUSTOMPLOT_USE_OPENGL
)
target_include_directories(test_theme PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
)
set_target_properties(test_theme PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_theme COMMAND test_theme)
