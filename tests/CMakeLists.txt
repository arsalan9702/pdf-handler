find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(pdfcore_tests
    test_smoke.cpp
)
target_link_libraries(pdfcore_tests PRIVATE pdfh::core Catch2::Catch2WithMain)
catch_discover_tests(pdfcore_tests)
