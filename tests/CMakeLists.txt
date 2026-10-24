find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

# Fixtures are generated at build time, never committed.
add_executable(make_fixtures fixtures/gen/make_fixtures.cpp)
target_link_libraries(make_fixtures PRIVATE qpdf::libqpdf)

set(PDFH_FIXTURE_DIR ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
add_custom_command(
    OUTPUT ${PDFH_FIXTURE_DIR}/.stamp
    COMMAND make_fixtures ${PDFH_FIXTURE_DIR}
    COMMAND ${CMAKE_COMMAND} -E touch ${PDFH_FIXTURE_DIR}/.stamp
    DEPENDS make_fixtures
    COMMENT "Generating PDF test fixtures"
    VERBATIM)
add_custom_target(pdfh_fixtures DEPENDS ${PDFH_FIXTURE_DIR}/.stamp)

find_program(PDFH_QPDF_EXECUTABLE qpdf)
if(NOT PDFH_QPDF_EXECUTABLE)
    message(WARNING "qpdf CLI not found: tests fall back to the in-library check")
    set(PDFH_QPDF_EXECUTABLE "")
endif()

add_executable(pdfcore_tests
    test_smoke.cpp
    test_page_range.cpp
    test_document.cpp
    test_page_copy.cpp
    test_ops.cpp
)
add_dependencies(pdfcore_tests pdfh_fixtures)
target_link_libraries(pdfcore_tests PRIVATE pdfh::core Catch2::Catch2WithMain)
target_compile_definitions(pdfcore_tests PRIVATE
    PDFH_FIXTURE_DIR="${PDFH_FIXTURE_DIR}"
    PDFH_QPDF_EXECUTABLE="${PDFH_QPDF_EXECUTABLE}")
catch_discover_tests(pdfcore_tests)

# App layer (models, undo, tools facade, job runner), driven without QML.
if(PDFH_BUILD_APP)
    find_package(Qt6 6.9 REQUIRED COMPONENTS Quick Pdf Test)
    set(APP_DIR ${PROJECT_SOURCE_DIR}/app)
    add_executable(app_tests
        app/test_app.cpp
        ${APP_DIR}/commands/PageCommands.cpp
        ${APP_DIR}/models/DocumentListModel.cpp
        ${APP_DIR}/models/ImageListModel.cpp
        ${APP_DIR}/models/ProjectPageModel.cpp
        ${APP_DIR}/services/JobRunner.cpp
        ${APP_DIR}/services/PdfTools.cpp
        ${APP_DIR}/services/QImageRecoder.cpp
        ${APP_DIR}/services/ThumbnailProvider.cpp
    )
    set_target_properties(app_tests PROPERTIES AUTOMOC ON)
    add_dependencies(app_tests pdfh_fixtures)
    target_include_directories(app_tests PRIVATE ${APP_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(app_tests PRIVATE pdfh::core Qt6::Quick Qt6::Pdf Qt6::Test Catch2::Catch2)
    target_compile_definitions(app_tests PRIVATE
        PDFH_FIXTURE_DIR="${PDFH_FIXTURE_DIR}"
        PDFH_QPDF_EXECUTABLE="${PDFH_QPDF_EXECUTABLE}")
    catch_discover_tests(app_tests DISCOVERY_MODE PRE_TEST
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
