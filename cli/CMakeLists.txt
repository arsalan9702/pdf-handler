add_library(pdfh_cli STATIC Cli.cpp)
target_link_libraries(pdfh_cli PUBLIC pdfh::core)
if(WIN32)
    target_link_libraries(pdfh_cli PRIVATE shell32)
endif()

add_executable(pdfh main.cpp)
target_link_libraries(pdfh PRIVATE pdfh_cli)
install(TARGETS pdfh RUNTIME DESTINATION bin)
