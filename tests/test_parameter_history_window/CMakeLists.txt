# Telemetry history window: samples -> curve, shared capacity spin box, CSV export.
add_executable(test_parameter_history_window)
target_sources(test_parameter_history_window PRIVATE
    test_parameter_history_window.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/Theme.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/qcustomplot.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config/ParameterHistoryStore.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/LineViewWidget.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display/ParameterHistoryWindow.cpp
)
target_link_libraries(test_parameter_history_window PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test Qt6::OpenGL Qt6::PrintSupport
    OpenGL::GL
)
target_compile_definitions(test_parameter_history_window PRIVATE QCUSTOMPLOT_USE_OPENGL)
target_include_directories(test_parameter_history_window PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/widgets
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/display
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/config
)
set_target_properties(test_parameter_history_window PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_parameter_history_window COMMAND test_parameter_history_window)
