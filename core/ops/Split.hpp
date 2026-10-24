#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace pdfcore {

enum class SplitMode {
    EveryNPages, // parts of `everyN` pages; the last part may be shorter
    EachPage,    // one file per page
    Ranges,      // one file per range string in `ranges`
};

struct SplitOptions {
    SplitMode mode = SplitMode::EachPage;
    int everyN = 1;
    std::vector<std::string> ranges;
    // {name} = source file stem, {n} = part number (zero-padded), {pages} = page range.
    std::string nameTemplate = "{name}-{n}";
    bool overwrite = false; // otherwise existing files are an error before anything is written
    WriteOptions write;
};

// Splits `source` into several files in `outputDir`. Either every part is written or, on
// failure or cancel, the parts written so far are removed again.
OpResult splitDocument(const SourceDoc& source, const std::filesystem::path& outputDir,
                       const SplitOptions& options = {}, const JobContext& ctx = {});

// The file names splitDocument would produce, for previews in the UI.
std::vector<std::filesystem::path> planSplit(const SourceDoc& source,
                                             const std::filesystem::path& outputDir,
                                             const SplitOptions& options);

} // namespace pdfcore
