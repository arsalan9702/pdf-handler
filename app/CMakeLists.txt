find_package(Qt6 6.9 REQUIRED COMPONENTS Quick QuickControls2 Pdf Svg)
include(GNUInstallDirs)
qt_standard_project_setup(REQUIRES 6.9)

qt_add_executable(pdf-handler
    main.cpp
)

# App icon in every platform format, rendered from the SVG at build time.
add_executable(pdfh_icon_gen ${PROJECT_SOURCE_DIR}/tools/icon_gen.cpp)
target_link_libraries(pdfh_icon_gen PRIVATE Qt6::Gui Qt6::Svg)
set(PDFH_ICON_SVG ${CMAKE_CURRENT_SOURCE_DIR}/qml/brand/app-icon.svg)
set(PDFH_ICON_DIR ${CMAKE_CURRENT_BINARY_DIR}/icons)
add_custom_command(
    OUTPUT ${PDFH_ICON_DIR}/app.ico ${PDFH_ICON_DIR}/png/256.png
    COMMAND pdfh_icon_gen ${PDFH_ICON_SVG} ${PDFH_ICON_DIR}
    DEPENDS pdfh_icon_gen ${PDFH_ICON_SVG}
    COMMENT "Rendering app icons"
    VERBATIM)
add_custom_target(pdfh_icons DEPENDS ${PDFH_ICON_DIR}/app.ico)
add_dependencies(pdf-handler pdfh_icons)

if(WIN32)
    configure_file(${PROJECT_SOURCE_DIR}/packaging/windows/app.rc.in
                   ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    target_sources(pdf-handler PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/app.rc PROPERTIES
        OBJECT_DEPENDS ${PDFH_ICON_DIR}/app.ico)
elseif(APPLE)
    add_custom_command(
        OUTPUT ${PDFH_ICON_DIR}/app.icns
        COMMAND iconutil -c icns ${PDFH_ICON_DIR}/app.iconset -o ${PDFH_ICON_DIR}/app.icns
        DEPENDS ${PDFH_ICON_DIR}/app.ico
        VERBATIM)
    target_sources(pdf-handler PRIVATE ${PDFH_ICON_DIR}/app.icns)
    set_source_files_properties(${PDFH_ICON_DIR}/app.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources GENERATED TRUE)
endif()

set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# The UI (C++ types + QML) is a static QML module so tests can load the real interface.
qt_add_library(pdfh_ui STATIC)
qt_add_qml_module(pdfh_ui
    URI PdfHandler
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/PdfHandler
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
target_include_directories(pdfh_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/models ${CMAKE_CURRENT_SOURCE_DIR}/services
    ${CMAKE_CURRENT_SOURCE_DIR}/commands)
target_link_libraries(pdfh_ui PUBLIC
    pdfh::core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Pdf
    Qt6::Svg
)

set_target_properties(pdf-handler PROPERTIES
    OUTPUT_NAME pdf-handler
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.pdfhandler
    MACOSX_BUNDLE_BUNDLE_NAME "PDF Handler"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in
)

if(APPLE)
    set_target_properties(pdf-handler PROPERTIES OUTPUT_NAME "PDF Handler") # "PDF Handler.app"
endif()

target_compile_definitions(pdf-handler PRIVATE PDFH_VERSION="${PROJECT_VERSION}")

target_link_libraries(pdf-handler PRIVATE pdfh_ui pdfh_uiplugin pdfh_cli)

install(TARGETS pdf-handler
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32 OR APPLE)
    # windeployqt / macdeployqt: Qt libraries, plugins and QML imports next to the app.
    qt_generate_deploy_qml_app_script(
        TARGET pdf-handler
        OUTPUT_SCRIPT deploy_script
        NO_TRANSLATIONS
    )
    install(SCRIPT ${deploy_script})
else()
    # Linux: desktop entry, metadata and icons; the AppImage bundles Qt via linuxdeploy.
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/io.github.pdfhandler.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/io.github.pdfhandler.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(FILES ${PDFH_ICON_SVG} DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME io.github.pdfhandler.svg)
    foreach(size 16 32 48 64 128 256 512)
        install(FILES ${PDFH_ICON_DIR}/png/${size}.png
                DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps
                RENAME io.github.pdfhandler.png)
    endforeach()
endif()
