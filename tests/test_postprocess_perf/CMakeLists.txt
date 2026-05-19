# Performance tests for PostProcess functions (verticalBinning)
add_executable(test_postprocess_perf)
target_sources(test_postprocess_perf PRIVATE
    test_postprocess_perf.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/PostProcess.cpp
)
target_link_libraries(test_postprocess_perf PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
)
target_include_directories(test_postprocess_perf PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gui/widgets
)
add_test(NAME test_postprocess_perf COMMAND test_postprocess_perf)
