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
    MainWindowUi.h MainWindowUi.cpp
    CameraTab.h CameraTab.cpp
    ImageViewWidget.h ImageViewWidget.cpp
    SpectrumViewWidget.h SpectrumViewWidget.cpp
    qcustomplot.h qcustomplot.cpp
)