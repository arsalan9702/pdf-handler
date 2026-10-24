find_package(Qt6 6.8 REQUIRED COMPONENTS Quick QuickControls2 Pdf Svg)
include(GNUInstallDirs)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(pdf-handler
    main.cpp
)

set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(pdf-handler
    URI PdfHandler
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
)

set_target_properties(pdf-handler PROPERTIES
    OUTPUT_NAME pdf-handler
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.pdfhandler
    MACOSX_BUNDLE_BUNDLE_NAME "PDF Handler"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)

target_compile_definitions(pdf-handler PRIVATE PDFH_VERSION="${PROJECT_VERSION}")

target_link_libraries(pdf-handler PRIVATE
    pdfh::core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Pdf
    Qt6::Svg
)

install(TARGETS pdf-handler
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET pdf-handler
    OUTPUT_SCRIPT deploy_script
)
install(SCRIPT ${deploy_script})
