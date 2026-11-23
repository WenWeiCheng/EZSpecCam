# Pure GUI services: statistics, histogram binning, history CSV and file naming.
# These must stay linkable without QtWidgets or QCustomPlot.
add_executable(test_gui_services)
target_sources(test_gui_services PRIVATE
    test_gui_services.cpp
    test_gui_services.h
    ${CMAKE_SOURCE_DIR}/src/gui/services/FileNaming.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/services/HistogramBinner.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/services/HistoryCsvExporter.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/services/ImageStatistics.cpp
)
target_link_libraries(test_gui_services PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Test
)
target_include_directories(test_gui_services PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gui
    ${CMAKE_SOURCE_DIR}/src/gui/services
)
set_target_properties(test_gui_services PROPERTIES
    Qt6::MinimalQt6 TRUE
)
add_test(NAME test_gui_services COMMAND test_gui_services)
