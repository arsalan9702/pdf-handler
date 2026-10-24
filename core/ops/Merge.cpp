#include "core/ops/Merge.hpp"

#include "core/Document.hpp"
#include "core/PageCopy.hpp"
#include "core/ops/Extract.hpp"

namespace pdfcore {

OpResult mergeDocuments(const std::vector<MergeInput>& inputs, const std::filesystem::path& output,
                        const MergeOptions& options, const JobContext& ctx) {
    if (inputs.empty()) {
        throw PdfError(ErrorKind::InvalidArgument, "Add at least one document to merge");
    }
    Project project;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        const auto& in = inputs[i];
        const auto id = static_cast<DocId>(i);
        SourceDoc src = in.doc;
        src.id = id;
        const auto indices = resolveRange(src, in.range);
        if (options.bookmarkPerFile) {
            project.bookmarks.push_back(
                {pathToUtf8(src.path.stem()), static_cast<int>(project.pages.size())});
        }
        for (int page : indices) {
            project.pages.push_back({id, page, 0});
        }
        project.sources.push_back(std::move(src));
    }
    return writePages(project, output, options.write, ctx);
}

} // namespace pdfcore
