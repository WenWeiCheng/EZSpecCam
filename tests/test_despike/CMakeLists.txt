# 去尖峰算法：单像素尖峰被替换、宽特征与凹陷不动、次阈值判据
add_executable(test_despike)
target_sources(test_despike PRIVATE
    test_despike.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/widgets/PostProcess.cpp
)
target_link_libraries(test_despike PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Test
)
target_include_directories(test_despike PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/gui/widgets
)
add_test(NAME test_despike COMMAND test_despike)
