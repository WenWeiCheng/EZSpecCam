# EZSpecCam CLI Application
# Headless camera control via command-line interface

add_executable(ezspeccam_cli)

target_include_directories(ezspeccam_cli PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ezspeccam_cli PRIVATE
    EZSpecCam::Core
    Qt6::Core
)

target_sources(ezspeccam_cli PRIVATE
    main.cpp
    SequenceRunner.h
    SequenceRunner.cpp
)

# Deploy mock plugin for CLI testing
add_custom_command(TARGET ezspeccam_cli POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ezspeccam_cli>/plugins/drivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:mock_camera_driver>"
        "$<TARGET_FILE_DIR:ezspeccam_cli>/plugins/drivers/"
    COMMENT "Deploying mock_camera_driver for CLI"
)
