#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <string_view>

namespace pdfcore {

// Number of pages in a document (opens it briefly). Throws PdfError like openDocument.
int countPages(const SourceDoc& source);

// Parses a 1-based range string against `source`, throwing PdfError(InvalidArgument) with
// the parser's message on bad input. An empty range means every page.
std::vector<int> resolveRange(const SourceDoc& source, std::string_view range);

// Writes the pages in `range` (in the order written) to `output`.
OpResult extractPages(const SourceDoc& source, std::string_view range,
                      const std::filesystem::path& output, const WriteOptions& options = {},
                      const JobContext& ctx = {});

// Writes every page except those in `range`.
OpResult deletePages(const SourceDoc& source, std::string_view range,
                     const std::filesystem::path& output, const WriteOptions& options = {},
                     const JobContext& ctx = {});

// Keeps every page, rotating those in `range` (empty = all) clockwise by `degrees`.
OpResult rotatePages(const SourceDoc& source, std::string_view range, int degrees,
                     const std::filesystem::path& output, const WriteOptions& options = {},
                     const JobContext& ctx = {});

} // namespace pdfcore
