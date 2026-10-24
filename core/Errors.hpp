#pragma once

#include <stdexcept>
#include <string>

namespace pdfcore {

enum class ErrorKind {
    InvalidArgument,  // caller passed something nonsensical (bad range, unknown source id)
    Io,               // file missing, unreadable, unwritable
    NotAPdf,          // no %PDF- header at all
    PasswordRequired, // encrypted and no password given
    WrongPassword,    // encrypted and the given password did not match
    Damaged,          // QPDF could not recover the file
    Unsupported,      // PDF feature QPDF does not handle
    ValidationFailed, // output failed the post-write check; nothing was replaced
    Cancelled,        // CancelToken was triggered
};

class PdfError : public std::runtime_error {
  public:
    PdfError(ErrorKind kind, const std::string& message)
        : std::runtime_error(message), m_kind(kind) {}

    [[nodiscard]] ErrorKind kind() const noexcept { return m_kind; }

  private:
    ErrorKind m_kind;
};

} // namespace pdfcore
