add_library(pdfcore STATIC
    Version.cpp
)
add_library(pdfh::core ALIAS pdfcore)

target_include_directories(pdfcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(pdfcore PUBLIC qpdf::libqpdf)
target_compile_features(pdfcore PUBLIC cxx_std_23)
