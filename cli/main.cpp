#include "core/Version.hpp"

#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc > 1 && std::string_view(argv[1]) == "--version") {
        std::cout << "pdfh 0.1.0 (qpdf " << pdfcore::qpdfVersion() << ")\n";
        return 0;
    }
    std::cerr << "usage: pdfh --version\n";
    return 1;
}
