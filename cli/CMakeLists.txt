add_executable(pdfh main.cpp)
target_link_libraries(pdfh PRIVATE pdfh::core)
install(TARGETS pdfh RUNTIME DESTINATION bin)
