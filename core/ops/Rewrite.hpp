#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <functional>

class QPDF;

namespace pdfcore {

// Callback that changes the open document in memory. `pageCtx` reports progress in pages
// (done, pageCount) and should be checked for cancellation once per page.
using DocumentModifier =
    std::function<void(QPDF& pdf, int pageCount, OpResult& result, const JobContext& pageCtx)>;

// Shared path for whole-document operations (encrypt, decrypt, metadata, compress):
// open `source`, apply `modify`, then write atomically with validation. `output` may equal
// source.path. Progress is reported in 2 * pageCount units.
OpResult rewriteDocument(const SourceDoc& source, const std::filesystem::path& output,
                         const WriteOptions& options, const JobContext& ctx,
                         const DocumentModifier& modify);

} // namespace pdfcore
