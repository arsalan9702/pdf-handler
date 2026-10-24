#pragma once

#include "core/Types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class QPDF;

namespace pdfcore {

struct Permissions {
    bool print = true;
    bool highQualityPrint = true;
    bool modify = true;
    bool extract = true;
    bool annotate = true;
    bool fillForms = true;
    bool assemble = true;
    bool accessibility = true;
};

struct Encryption {
    std::string userPassword;  // empty = anyone can open; permissions still apply
    std::string ownerPassword; // must be non-empty
    Permissions permissions;
};

enum class EncryptionMode {
    Preserve, // keep the source's encryption (single-source output only)
    Remove,   // write unencrypted
    Set,      // apply WriteOptions::encryption (AES-256)
};

enum class ObjectStreams { Preserve, Generate, Disable };

struct WriteOptions {
    EncryptionMode encryptionMode = EncryptionMode::Preserve;
    std::optional<Encryption> encryption;
    ObjectStreams objectStreams = ObjectStreams::Preserve;
    bool linearize = false;
    bool recompressFlate = false;
};

// A temp file next to the target that only replaces the target on commit().
// If commit() is never called (error, cancel), the destructor deletes the temp file
// and the target is untouched.
class AtomicOutput {
  public:
    explicit AtomicOutput(std::filesystem::path target);
    ~AtomicOutput();
    AtomicOutput(const AtomicOutput&) = delete;
    AtomicOutput& operator=(const AtomicOutput&) = delete;

    [[nodiscard]] const std::filesystem::path& tempPath() const { return m_temp; }
    [[nodiscard]] const std::filesystem::path& targetPath() const { return m_target; }

    // Flushes the temp file to disk and atomically replaces the target.
    // Any QPDF objects reading the target must be closed first (Windows locks open files).
    void commit();

  private:
    std::filesystem::path m_target;
    std::filesystem::path m_temp;
    bool m_committed = false;
};

struct ValidationResult {
    int pageCount = 0;
    std::vector<std::string> warnings;
};

// Runs the equivalent of `qpdf --check` on `path` plus an optional page-count check.
// Throws PdfError(ValidationFailed) on errors; warnings are returned, not thrown.
ValidationResult validatePdf(const std::filesystem::path& path, const std::string& password,
                             std::optional<int> expectedPages);

// Writes `pdf` to out.tempPath() and validates it. Does not commit, so callers can release
// their source documents first. `reopenPassword` is whatever opens the written file.
ValidationResult writeAndValidate(QPDF& pdf, AtomicOutput& out, const WriteOptions& options,
                                  int expectedPages, const std::string& reopenPassword,
                                  const JobContext& ctx);

// Password that opens a file written with these options, given the source password.
std::string reopenPasswordFor(const WriteOptions& options, const std::string& sourcePassword);

} // namespace pdfcore
