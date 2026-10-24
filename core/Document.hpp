#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class QPDF;

namespace pdfcore {

struct OpenedDocument {
    std::shared_ptr<QPDF> pdf;
    int pageCount = 0;
    bool encrypted = false;
    std::vector<std::string> warnings; // recovery/repair messages from opening
};

// Opens a PDF with QPDF's recovery enabled. Throws PdfError with a specific kind
// (NotAPdf, PasswordRequired, WrongPassword, Damaged, Io).
OpenedDocument openDocument(const std::filesystem::path& path, const std::string& password = {});

// Moves warnings QPDF has accumulated (objects are read lazily, so warnings keep arriving
// after open) into plain strings.
std::vector<std::string> takeWarnings(QPDF& pdf);

// QPDF expects UTF-8 file names on every platform, including Windows.
std::string pathToUtf8(const std::filesystem::path& path);

// Rethrows the current exception as a PdfError with a meaningful kind.
// Call from inside a catch block.
[[noreturn]] void rethrowAsPdfError(const std::string& context, bool passwordGiven);

} // namespace pdfcore
