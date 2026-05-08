# EZSpecCam Test Suite

add_subdirectory(test_configuration)
# add_subdirectory(test_post_process)  # DISABLED - PostProcess moved to MainWindow
add_subdirectory(test_mock_driver)
add_subdirectory(test_plugin_loading)
add_subdirectory(test_app_controller)
add_subdirectory(test_view_widgets_perf)
# add_subdirectory(test_qhyccd_driver)  # DISABLED - source files missing
# add_subdirectory(test_cli)  # TODO: fix CLI library linkage
add_subdirectory(test_profile_demo)
# add_subdirectory(test_rect_zoom_demo)