#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace pdfcore {

struct MergeInput {
    SourceDoc doc;
    std::string range; // 1-based range string; empty = all pages
};

struct MergeOptions {
    // Adds one top-level bookmark per input (titled with the file name) at its first page.
    bool bookmarkPerFile = false;
    WriteOptions write;
};

// Concatenates the inputs in order. The same file may appear more than once.
OpResult mergeDocuments(const std::vector<MergeInput>& inputs, const std::filesystem::path& output,
                        const MergeOptions& options = {}, const JobContext& ctx = {});

} // namespace pdfcore
