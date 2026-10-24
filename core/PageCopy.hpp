#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <memory>

namespace pdfcore {

// Source documents kept open across several writes, so producing many outputs from one big
// file (split) costs time proportional to each output, not to the source's size.
class SourceCache {
  public:
    SourceCache();
    ~SourceCache();
    SourceCache(const SourceCache&) = delete;
    SourceCache& operator=(const SourceCache&) = delete;

    struct Impl;
    std::unique_ptr<Impl> impl;
};

// Copies project.pages out of documents opened through `cache` (addPage path) and writes them
// via AtomicOutput. When every page comes from one source, its encryption (with
// EncryptionMode::Preserve) and document information carry over to the output.
OpResult copyPages(SourceCache& cache, const Project& project, const std::filesystem::path& output,
                   const WriteOptions& options = {}, const JobContext& ctx = {});

// The single page-copy path. Builds `output` from project.pages (in order, with rotations)
// and writes it via AtomicOutput, so a failure never leaves a partial or damaged file.
//
// One source document: QPDFJob page selection (keeps annotations, links, form fields and
// document-level structure). Several sources: QPDFPageDocumentHelper::addPage plus
// fixCopiedAnnotations so form fields keep working.
//
// Progress is reported in 2 * pageCount units: one per copied page, then the write.
OpResult writePages(const Project& project, const std::filesystem::path& output,
                    const WriteOptions& options = {}, const JobContext& ctx = {});

} // namespace pdfcore
