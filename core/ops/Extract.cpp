#include "core/ops/Extract.hpp"

#include "core/Document.hpp"
#include "core/PageCopy.hpp"
#include "core/PageRange.hpp"

#include <set>

namespace pdfcore {

namespace {

Project projectFor(const SourceDoc& source, const std::vector<int>& indices, int rotation = 0) {
    Project p;
    p.sources.push_back(source);
    p.sources.back().id = 0;
    for (int i : indices) {
        p.pages.push_back({0, i, rotation});
    }
    return p;
}

std::vector<int> allPages(int count) {
    std::vector<int> v(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        v[static_cast<std::size_t>(i)] = i;
    }
    return v;
}

} // namespace

int countPages(const SourceDoc& source) {
    return openDocument(source.path, source.password).pageCount;
}

std::vector<int> resolveRange(const SourceDoc& source, std::string_view range) {
    const int count = countPages(source);
    if (range.find_first_not_of(" \t") == std::string_view::npos) {
        return allPages(count);
    }
    auto parsed = parsePageRange(range, count);
    if (!parsed) {
        throw PdfError(ErrorKind::InvalidArgument, parsed.error().message);
    }
    return *parsed;
}

OpResult extractPages(const SourceDoc& source, std::string_view range,
                      const std::filesystem::path& output, const WriteOptions& options,
                      const JobContext& ctx) {
    return writePages(projectFor(source, resolveRange(source, range)), output, options, ctx);
}

OpResult deletePages(const SourceDoc& source, std::string_view range,
                     const std::filesystem::path& output, const WriteOptions& options,
                     const JobContext& ctx) {
    if (range.find_first_not_of(" \t") == std::string_view::npos) {
        throw PdfError(ErrorKind::InvalidArgument, "Choose which pages to delete");
    }
    const auto doomedList = resolveRange(source, range);
    const std::set<int> doomed(doomedList.begin(), doomedList.end());
    std::vector<int> keep;
    for (int i = 0, n = countPages(source); i < n; ++i) {
        if (!doomed.contains(i)) {
            keep.push_back(i);
        }
    }
    if (keep.empty()) {
        throw PdfError(ErrorKind::InvalidArgument, "Cannot delete every page of a document");
    }
    return writePages(projectFor(source, keep), output, options, ctx);
}

OpResult rotatePages(const SourceDoc& source, std::string_view range, int degrees,
                     const std::filesystem::path& output, const WriteOptions& options,
                     const JobContext& ctx) {
    const auto selectedList = resolveRange(source, range);
    const std::set<int> selected(selectedList.begin(), selectedList.end());
    auto project = projectFor(source, allPages(countPages(source)));
    for (auto& ref : project.pages) {
        if (selected.contains(ref.pageIndex)) {
            ref.rotation = degrees;
        }
    }
    return writePages(project, output, options, ctx);
}

} // namespace pdfcore
