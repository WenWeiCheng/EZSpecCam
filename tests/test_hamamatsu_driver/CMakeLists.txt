# Test Hamamatsu driver
add_executable(test_hamamatsu_driver)
target_sources(test_hamamatsu_driver PRIVATE
    test_hamamatsu_driver.cpp
    ${CMAKE_SOURCE_DIR}/src/plugins/hamamatsu/HamamatsuDriver.cpp
)
target_link_libraries(test_hamamatsu_driver PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test
    "${CMAKE_SOURCE_DIR}/src/plugins/hamamatsu/sdk/winlib/x64/dcamapi.lib"
)
target_include_directories(test_hamamatsu_driver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/hamamatsu
    "${CMAKE_SOURCE_DIR}/src/plugins/hamamatsu/sdk/include"
)
set_target_properties(test_hamamatsu_driver PROPERTIES
    Qt6::MinimalQt6 TRUE
)
# add_test(NAME test_hamamatsu_driver COMMAND test_hamamatsu_driver)  # requires HAMAMATSU camera hardware
