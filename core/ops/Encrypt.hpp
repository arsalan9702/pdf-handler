#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <filesystem>

namespace pdfcore {

// Protects `source` with AES-256 (PDF 2.0 / R6). `source.password` opens the input if it is
// already encrypted; the old protection is replaced.
OpResult encryptDocument(const SourceDoc& source, const Encryption& encryption,
                         const std::filesystem::path& output, const JobContext& ctx = {});

// Removes all protection. `source.password` must be the user or owner password.
OpResult decryptDocument(const SourceDoc& source, const std::filesystem::path& output,
                         const JobContext& ctx = {});

} // namespace pdfcore
