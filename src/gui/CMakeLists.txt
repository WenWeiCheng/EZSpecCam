# GUI application
qt_add_executable(ezspeccam_gui)

target_link_libraries(ezspeccam_gui PRIVATE
    ezspeccam_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::PrintSupport Qt6::OpenGL
    OpenGL::GL
)

target_compile_definitions(ezspeccam_gui PRIVATE QCUSTOMPLOT_USE_OPENGL)

target_sources(ezspeccam_gui PRIVATE
    main.cpp
    AppController.h AppController.cpp
    MainWindow.h MainWindow.cpp
    ui/MainWindowUi.h ui/MainWindowUi.cpp
    widgets/CameraTab.h widgets/CameraTab.cpp
    widgets/ImageViewWidget.h widgets/ImageViewWidget.cpp
    widgets/SpectrumViewWidget.h widgets/SpectrumViewWidget.cpp
    qcustomplot.h qcustomplot.cpp
)