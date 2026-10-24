#include "core/ops/Rewrite.hpp"

#include "core/Document.hpp"

#include <qpdf/QPDF.hh>

namespace pdfcore {

OpResult rewriteDocument(const SourceDoc& source, const std::filesystem::path& output,
                         const WriteOptions& options, const JobContext& ctx,
                         const DocumentModifier& modify) {
    OpResult result;
    AtomicOutput out(output);
    {
        auto doc = openDocument(source.path, source.password);
        appendWarnings(result.warnings, doc.warnings);
        const int n = doc.pageCount;
        ctx.checkCancel();
        ctx.report(0, 2 * n);
        if (modify) {
            try {
                modify(*doc.pdf, n, result, ctx.slice(0, n, 2 * n));
            } catch (...) {
                rethrowAsPdfError(pathToUtf8(source.path), !source.password.empty());
            }
        }
        ctx.report(n, 2 * n);
        auto validation =
            writeAndValidate(*doc.pdf, out, options, n, reopenPasswordFor(options, source.password),
                             ctx.slice(n, n, 2 * n));
        appendWarnings(result.warnings, takeWarnings(*doc.pdf));
        appendWarnings(result.warnings, validation.warnings);
    } // source closes before the target is replaced
    out.commit();
    result.outputs.push_back(output);
    return result;
}

} // namespace pdfcore
