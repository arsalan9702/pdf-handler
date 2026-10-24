cmake_minimum_required(VERSION 3.24)

project(PdfHandler
    VERSION 0.1.0
    DESCRIPTION "Cross-platform desktop PDF toolkit"
    LANGUAGES CXX)

# C++23 for std::expected (see CLAUDE.md: prefer std::expected for parse results).
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PDFH_BUILD_APP "Build the Qt Quick application" ON)
option(PDFH_BUILD_CLI "Build the pdfh command-line tool" ON)
option(PDFH_BUILD_TESTS "Build the Catch2 test suite" ON)
option(PDFH_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
option(PDFH_FUZZ "Build the libFuzzer target (clang only)" OFF)

if(PDFH_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(qpdf CONFIG REQUIRED)
# vcpkg's qpdf config lists its deps as bare "z;jpeg", which makes the linker search system
# paths (missing on CI, and the wrong copy on dev machines). Point it at vcpkg's own targets.
find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
set_target_properties(qpdf::libqpdf PROPERTIES INTERFACE_LINK_LIBRARIES "ZLIB::ZLIB;JPEG::JPEG")

add_subdirectory(core)

if(PDFH_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(PDFH_BUILD_APP)
    add_subdirectory(app)
endif()

if(PDFH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
