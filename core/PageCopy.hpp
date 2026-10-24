#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <filesystem>

namespace pdfcore {

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
