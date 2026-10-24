#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <string>

namespace pdfcore {

// Document information dictionary fields. Empty strings mean "not set".
struct Metadata {
    std::string title;
    std::string author;
    std::string subject;
    std::string keywords;
    std::string creator;
    std::string producer;
    std::string creationDate; // PDF date string, e.g. D:20260101120000Z (read-only here)
    std::string modDate;      // PDF date string (set automatically on write)

    bool operator==(const Metadata&) const = default;
};

struct DocumentInfo {
    int pageCount = 0;
    bool encrypted = false;
    std::string pdfVersion;
    bool hasXmpMetadata = false;
    Metadata metadata;
    std::vector<std::string> warnings;
};

DocumentInfo readDocumentInfo(const SourceDoc& source);

// Replaces title, author, subject, keywords, creator and producer with `metadata`'s values
// (empty = remove) and sets ModDate to now. Encryption is preserved.
OpResult writeMetadata(const SourceDoc& source, const Metadata& metadata,
                       const std::filesystem::path& output, const JobContext& ctx = {});

} // namespace pdfcore
