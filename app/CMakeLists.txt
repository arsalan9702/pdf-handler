find_package(Qt6 6.9 REQUIRED COMPONENTS Quick QuickControls2 Pdf Svg)
include(GNUInstallDirs)
qt_standard_project_setup(REQUIRES 6.9)

qt_add_executable(pdf-handler
    main.cpp
)

set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(pdf-handler
    URI PdfHandler
    SOURCES
        commands/PageCommands.cpp commands/PageCommands.hpp
        models/DocumentListModel.cpp models/DocumentListModel.hpp
        models/ImageListModel.cpp models/ImageListModel.hpp
        models/ProjectPageModel.cpp models/ProjectPageModel.hpp
        services/AppController.cpp services/AppController.hpp
        services/IconProvider.cpp services/IconProvider.hpp
        services/JobRunner.cpp services/JobRunner.hpp
        services/PdfTools.cpp services/PdfTools.hpp
        services/QImageRecoder.cpp services/QImageRecoder.hpp
        services/ThumbnailProvider.cpp services/ThumbnailProvider.hpp
    QML_FILES
        qml/Main.qml
        qml/components/AppButton.qml
        qml/components/AppCheckBox.qml
        qml/components/AppIcon.qml
        qml/components/AppSlider.qml
        qml/components/AppTextField.qml
        qml/components/Card.qml
        qml/components/DocumentPanel.qml
        qml/components/DocumentRow.qml
        qml/components/DropZone.qml
        qml/components/FieldLabel.qml
        qml/components/HelpText.qml
        qml/components/IconButton.qml
        qml/components/JobBar.qml
        qml/components/NavHeading.qml
        qml/components/NavItem.qml
        qml/components/OptionCard.qml
        qml/components/PageHeader.qml
        qml/components/PagePreview.qml
        qml/components/PageTile.qml
        qml/components/PasswordField.qml
        qml/components/SectionTitle.qml
        qml/components/SegmentedControl.qml
        qml/components/Toast.qml
        qml/components/ToastHost.qml
        qml/components/ToolCard.qml
        qml/components/ToolLayout.qml
        qml/pages/CompressPage.qml
        qml/pages/HomePage.qml
        qml/pages/ImagesToPdfPage.qml
        qml/pages/MergePage.qml
        qml/pages/OrganizePage.qml
        qml/pages/PdfToImagesPage.qml
        qml/pages/PropertiesPage.qml
        qml/pages/ProtectPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/SplitPage.qml
        qml/pages/UnlockPage.qml
        qml/theme/Theme.qml
    RESOURCES
        qml/brand/app-icon.svg
        qml/fonts/InterVariable.ttf
        qml/icons/arrow-down.svg
        qml/icons/arrow-up.svg
        qml/icons/check.svg
        qml/icons/chevron-down.svg
        qml/icons/chevron-up.svg
        qml/icons/circle-check.svg
        qml/icons/circle-x.svg
        qml/icons/copy.svg
        qml/icons/download.svg
        qml/icons/external-link.svg
        qml/icons/eye.svg
        qml/icons/file-image.svg
        qml/icons/file-pen.svg
        qml/icons/file-plus.svg
        qml/icons/file-text.svg
        qml/icons/files.svg
        qml/icons/folder-open.svg
        qml/icons/grip-vertical.svg
        qml/icons/house.svg
        qml/icons/image.svg
        qml/icons/images.svg
        qml/icons/info.svg
        qml/icons/key-round.svg
        qml/icons/layout-grid.svg
        qml/icons/loader-circle.svg
        qml/icons/lock-open.svg
        qml/icons/lock.svg
        qml/icons/monitor.svg
        qml/icons/moon.svg
        qml/icons/plus.svg
        qml/icons/redo-2.svg
        qml/icons/rotate-ccw.svg
        qml/icons/rotate-cw.svg
        qml/icons/scissors.svg
        qml/icons/settings.svg
        qml/icons/shrink.svg
        qml/icons/square-check-big.svg
        qml/icons/square.svg
        qml/icons/sun.svg
        qml/icons/trash-2.svg
        qml/icons/triangle-alert.svg
        qml/icons/undo-2.svg
        qml/icons/upload.svg
        qml/icons/x.svg
)

# qmltyperegistrar includes QML_ELEMENT headers by file name only.
target_include_directories(pdf-handler PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/commands)

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
    pdfh_cli
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
