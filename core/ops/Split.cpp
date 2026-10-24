#include "core/ops/Split.hpp"

#include "core/Document.hpp"
#include "core/PageCopy.hpp"
#include "core/PageRange.hpp"
#include "core/ops/Extract.hpp"

#include <algorithm>

namespace pdfcore {

namespace {

struct Part {
    std::vector<int> pages;
    std::filesystem::path path;
};

void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    for (std::size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size()) {
        s.replace(pos, from.size(), to);
    }
}

std::vector<Part> makeParts(const SourceDoc& source, const std::filesystem::path& outputDir,
                            const SplitOptions& options) {
    const int count = countPages(source);
    std::vector<std::vector<int>> groups;
    switch (options.mode) {
    case SplitMode::EachPage:
    case SplitMode::EveryNPages: {
        const int n = options.mode == SplitMode::EachPage ? 1 : options.everyN;
        if (n < 1) {
            throw PdfError(ErrorKind::InvalidArgument, "Pages per file must be at least 1");
        }
        for (int start = 0; start < count; start += n) {
            std::vector<int> g;
            for (int i = start; i < std::min(count, start + n); ++i) {
                g.push_back(i);
            }
            groups.push_back(std::move(g));
        }
        break;
    }
    case SplitMode::Ranges:
        if (options.ranges.empty()) {
            throw PdfError(ErrorKind::InvalidArgument, "Add at least one page range");
        }
        for (const auto& r : options.ranges) {
            auto parsed = parsePageRange(r, count);
            if (!parsed) {
                throw PdfError(ErrorKind::InvalidArgument,
                               "Range \"" + r + "\": " + parsed.error().message);
            }
            groups.push_back(std::move(*parsed));
        }
        break;
    }

    const std::string stem = pathToUtf8(source.path.stem());
    const int width = static_cast<int>(std::to_string(groups.size()).size());
    std::vector<Part> parts;
    for (std::size_t i = 0; i < groups.size(); ++i) {
        std::string number = std::to_string(i + 1);
        number.insert(0, static_cast<std::size_t>(width) - number.size(), '0');
        std::string name = options.nameTemplate.empty() ? "{name}-{n}" : options.nameTemplate;
        replaceAll(name, "{name}", stem);
        replaceAll(name, "{n}", number);
        replaceAll(name, "{pages}", formatPageRange(groups[i]));
        if (name.find_first_of("/\\:*?\"<>|") != std::string::npos) {
            throw PdfError(ErrorKind::InvalidArgument,
                           "File name contains characters that are not allowed: " + name);
        }
        name += ".pdf";
        parts.push_back(
            {groups[i],
             outputDir / std::filesystem::path(reinterpret_cast<const char8_t*>(name.c_str()))});
    }
    for (std::size_t i = 0; i < parts.size(); ++i) {
        for (std::size_t j = i + 1; j < parts.size(); ++j) {
            if (parts[i].path == parts[j].path) {
                throw PdfError(ErrorKind::InvalidArgument,
                               "The name pattern gives several parts the same file name; "
                               "include {n} in it");
            }
        }
    }
    return parts;
}

} // namespace

std::vector<std::filesystem::path> planSplit(const SourceDoc& source,
                                             const std::filesystem::path& outputDir,
                                             const SplitOptions& options) {
    std::vector<std::filesystem::path> out;
    for (auto& p : makeParts(source, outputDir, options)) {
        out.push_back(std::move(p.path));
    }
    return out;
}

OpResult splitDocument(const SourceDoc& source, const std::filesystem::path& outputDir,
                       const SplitOptions& options, const JobContext& ctx) {
    auto parts = makeParts(source, outputDir, options);
    if (!options.overwrite) {
        for (const auto& part : parts) {
            std::error_code ec;
            if (std::filesystem::exists(part.path, ec)) {
                throw PdfError(ErrorKind::Io,
                               "File already exists: " + pathToUtf8(part.path.filename()));
            }
        }
    }

    const int total = static_cast<int>(parts.size());
    OpResult result;
    SourceCache cache; // open the source once for all parts
    std::vector<std::filesystem::path> written;
    try {
        for (int i = 0; i < total; ++i) {
            const auto& part = parts[static_cast<std::size_t>(i)];
            ctx.checkCancel();
            Project p;
            p.sources.push_back(source);
            p.sources.back().id = 0;
            for (int page : part.pages) {
                p.pages.push_back({0, page, 0});
            }
            auto r =
                copyPages(cache, p, part.path, options.write, ctx.slice(i * 100, 100, total * 100));
            written.push_back(part.path);
            appendWarnings(result.warnings, r.warnings);
            ctx.report((i + 1) * 100, total * 100);
        }
    } catch (...) {
        // Without overwrite these files did not exist before, so removing them restores the
        // folder exactly. With overwrite, replaced files cannot be restored; keep them.
        if (!options.overwrite) {
            for (const auto& path : written) {
                std::error_code ec;
                std::filesystem::remove(path, ec);
            }
        }
        throw;
    }
    result.outputs = std::move(written);
    return result;
}

} // namespace pdfcore
