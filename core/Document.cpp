#include "core/Document.hpp"

#include "core/Errors.hpp"

#include <array>
#include <fstream>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <string_view>

namespace pdfcore {

namespace {

// QPDF tolerates junk before the header, but a real PDF has "%PDF-" in the first KB.
bool hasPdfHeader(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw PdfError(ErrorKind::Io, "Cannot open " + pathToUtf8(path));
    }
    std::array<char, 1024> buf{};
    in.read(buf.data(), buf.size());
    const std::string_view head(buf.data(), static_cast<std::size_t>(in.gcount()));
    return head.find("%PDF-") != std::string_view::npos;
}

} // namespace

std::string pathToUtf8(const std::filesystem::path& path) {
    const auto u8 = path.u8string();
    return {u8.begin(), u8.end()};
}

std::vector<std::string> takeWarnings(QPDF& pdf) {
    std::vector<std::string> out;
    for (const auto& w : pdf.getWarnings()) {
        out.emplace_back(w.what());
    }
    return out;
}

void rethrowAsPdfError(const std::string& context, bool passwordGiven) {
    try {
        throw;
    } catch (const PdfError&) {
        throw;
    } catch (const QPDFExc& e) {
        switch (e.getErrorCode()) {
        case qpdf_e_password:
            if (passwordGiven) {
                throw PdfError(ErrorKind::WrongPassword, context + ": incorrect password");
            }
            throw PdfError(ErrorKind::PasswordRequired, context + ": password required");
        case qpdf_e_system:
            throw PdfError(ErrorKind::Io, context + ": " + e.what());
        case qpdf_e_unsupported:
            throw PdfError(ErrorKind::Unsupported, context + ": " + e.what());
        default:
            throw PdfError(ErrorKind::Damaged, context + ": " + e.what());
        }
    } catch (const std::system_error& e) {
        throw PdfError(ErrorKind::Io, context + ": " + e.what());
    } catch (const std::exception& e) {
        // QUtil::safe_fopen throws std::runtime_error on open failure.
        throw PdfError(ErrorKind::Io, context + ": " + e.what());
    }
}

OpenedDocument openDocument(const std::filesystem::path& path, const std::string& password) {
    const std::string name = pathToUtf8(path);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        throw PdfError(ErrorKind::Io, "File not found: " + name);
    }
    if (!hasPdfHeader(path)) {
        throw PdfError(ErrorKind::NotAPdf, name + " is not a PDF file");
    }

    OpenedDocument doc;
    try {
        doc.pdf = QPDF::create();
        doc.pdf->setSuppressWarnings(true); // collected via takeWarnings, never printed
        doc.pdf->setAttemptRecovery(true);
        doc.pdf->processFile(name.c_str(), password.empty() ? nullptr : password.c_str());
        doc.encrypted = doc.pdf->isEncrypted();
        doc.pageCount = static_cast<int>(QPDFPageDocumentHelper(*doc.pdf).getAllPages().size());
    } catch (...) {
        rethrowAsPdfError(name, !password.empty());
    }
    if (doc.pageCount == 0) {
        throw PdfError(ErrorKind::Damaged, name + ": no pages could be found");
    }
    doc.warnings = takeWarnings(*doc.pdf);
    return doc;
}

} // namespace pdfcore
