#include "core/PageCopy.hpp"

#include "core/Document.hpp"
#include "core/PageRange.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <set>
#include <sstream>
#include <utility>

namespace pdfcore {

namespace {

int normalizedRotation(int degrees) {
    if (degrees % 90 != 0) {
        throw PdfError(ErrorKind::InvalidArgument,
                       "Rotation must be a multiple of 90, got " + std::to_string(degrees));
    }
    return ((degrees % 360) + 360) % 360;
}

const SourceDoc& findSource(const Project& project, DocId id) {
    for (const auto& s : project.sources) {
        if (s.id == id) {
            return s;
        }
    }
    throw PdfError(ErrorKind::InvalidArgument, "Unknown source document id " + std::to_string(id));
}

void appendUnique(std::vector<std::string>& into, const std::vector<std::string>& from) {
    for (const auto& w : from) {
        if (std::ranges::find(into, w) == into.end()) {
            into.push_back(w);
        }
    }
}

// Progress for the write phase maps 0..100% onto the second half of the unit range.
JobContext writePhase(const JobContext& ctx, int pages) {
    JobContext phase;
    phase.cancel = ctx.cancel;
    if (ctx.progress) {
        phase.progress = [&ctx, pages](int percent, int /*total*/) {
            ctx.report(pages + pages * percent / 100, 2 * pages);
        };
    }
    return phase;
}

OpResult copySingleSource(const Project& project, const SourceDoc& source,
                          const std::filesystem::path& output, const WriteOptions& options,
                          const JobContext& ctx) {
    const int n = static_cast<int>(project.pages.size());
    OpResult result;

    // Open once ourselves for precise errors (password, damage) and bounds checking.
    auto opened = openDocument(source.path, source.password);
    appendUnique(result.warnings, opened.warnings);
    std::vector<int> indices;
    std::map<int, std::vector<int>> outputPagesByAngle; // angle -> 0-based output positions
    for (int i = 0; i < n; ++i) {
        const auto& ref = project.pages[static_cast<std::size_t>(i)];
        if (ref.pageIndex < 0 || ref.pageIndex >= opened.pageCount) {
            throw PdfError(ErrorKind::InvalidArgument,
                           "Page " + std::to_string(ref.pageIndex + 1) + " does not exist");
        }
        indices.push_back(ref.pageIndex);
        if (const int angle = normalizedRotation(ref.rotation); angle != 0) {
            outputPagesByAngle[angle].push_back(i);
        }
    }
    opened.pdf.reset();
    ctx.checkCancel();
    ctx.report(0, 2 * n);

    AtomicOutput out(output);
    {
        const std::string name = pathToUtf8(source.path);
        const char* password = source.password.empty() ? nullptr : source.password.c_str();
        std::ostringstream log;
        auto logger = QPDFLogger::create();
        logger->setOutputStreams(&log, &log);

        QPDFJob job;
        job.setLogger(logger);
        std::unique_ptr<QPDF> pdf;
        try {
            auto config = job.config(); // keep the shared_ptr alive; methods return raw Config*
            config->inputFile(name);
            if (password != nullptr) {
                config->password(source.password);
            }
            config->pages()->pageSpec(".", formatPageRange(indices), password)->endPages();
            for (const auto& [angle, positions] : outputPagesByAngle) {
                config->rotate("+" + std::to_string(angle) + ":" + formatPageRange(positions));
            }
            // Required by checkConfiguration; the job never writes it, writeAndValidate does.
            config->outputFile(pathToUtf8(out.tempPath()));
            config->checkConfiguration();
            pdf = job.createQPDF();
        } catch (...) {
            rethrowAsPdfError(name, password != nullptr);
        }
        if (!pdf) {
            throw PdfError(ErrorKind::Damaged, name + ": could not select pages\n" + log.str());
        }
        pdf->setSuppressWarnings(true);
        appendUnique(result.warnings, takeWarnings(*pdf));
        ctx.report(n, 2 * n);

        auto validation = writeAndValidate(
            *pdf, out, options, n, reopenPasswordFor(options, source.password), writePhase(ctx, n));
        appendUnique(result.warnings, takeWarnings(*pdf));
        appendUnique(result.warnings, validation.warnings);
    } // job and pdf release the source file here, before the target may be replaced
    out.commit();
    result.outputs.push_back(output);
    return result;
}

struct OpenSource {
    OpenedDocument doc;
    std::vector<QPDFPageObjectHelper> pages;
    std::unique_ptr<QPDFAcroFormDocumentHelper> forms;
};

OpResult copyMultiSource(const Project& project, const std::filesystem::path& output,
                         const WriteOptions& requested, const JobContext& ctx) {
    const int n = static_cast<int>(project.pages.size());
    OpResult result;

    // A fresh document has no encryption to preserve.
    WriteOptions options = requested;
    if (options.encryptionMode == EncryptionMode::Preserve) {
        options.encryptionMode = EncryptionMode::Remove;
    }

    AtomicOutput out(output);
    {
        // Every source stays open until write() returns (QPDF copies lazily).
        std::map<DocId, OpenSource> sources;
        for (const auto& ref : project.pages) {
            if (sources.contains(ref.sourceDocId)) {
                continue;
            }
            const auto& src = findSource(project, ref.sourceDocId);
            OpenSource os;
            os.doc = openDocument(src.path, src.password);
            appendUnique(result.warnings, os.doc.warnings);
            os.pages = QPDFPageDocumentHelper(*os.doc.pdf).getAllPages();
            os.forms = std::make_unique<QPDFAcroFormDocumentHelper>(*os.doc.pdf);
            sources.emplace(ref.sourceDocId, std::move(os));
        }

        auto pdf = QPDF::create();
        pdf->setSuppressWarnings(true);
        pdf->emptyPDF();
        QPDFPageDocumentHelper outPages(*pdf);
        QPDFAcroFormDocumentHelper outForms(*pdf);

        ctx.report(0, 2 * n);
        try {
            for (int i = 0; i < n; ++i) {
                ctx.checkCancel();
                const auto& ref = project.pages[static_cast<std::size_t>(i)];
                auto& src = sources.at(ref.sourceDocId);
                if (ref.pageIndex < 0 || std::cmp_greater_equal(ref.pageIndex, src.pages.size())) {
                    throw PdfError(ErrorKind::InvalidArgument,
                                   "Page " + std::to_string(ref.pageIndex + 1) +
                                       " does not exist in " +
                                       pathToUtf8(findSource(project, ref.sourceDocId).path));
                }
                auto& srcPage = src.pages[static_cast<std::size_t>(ref.pageIndex)];
                outPages.addPage(srcPage, false);
                // addPage may shallow-copy duplicates, so fetch the page actually added.
                QPDFPageObjectHelper added(pdf->getAllPages().back());
                if (const int angle = normalizedRotation(ref.rotation); angle != 0) {
                    added.rotatePage(angle, true);
                }
                if (src.forms->hasAcroForm()) {
                    outForms.fixCopiedAnnotations(added.getObjectHandle(),
                                                  srcPage.getObjectHandle(), *src.forms);
                }
                ctx.report(i + 1, 2 * n);
            }
        } catch (...) {
            rethrowAsPdfError("Copying pages", false);
        }

        auto validation = writeAndValidate(*pdf, out, options, n, reopenPasswordFor(options, {}),
                                           writePhase(ctx, n));
        appendUnique(result.warnings, takeWarnings(*pdf));
        for (auto& [id, os] : sources) {
            appendUnique(result.warnings, takeWarnings(*os.doc.pdf));
        }
        appendUnique(result.warnings, validation.warnings);
    } // sources close here
    out.commit();
    result.outputs.push_back(output);
    return result;
}

} // namespace

OpResult writePages(const Project& project, const std::filesystem::path& output,
                    const WriteOptions& options, const JobContext& ctx) {
    if (project.pages.empty()) {
        throw PdfError(ErrorKind::InvalidArgument, "No pages selected");
    }
    std::set<DocId> used;
    for (const auto& ref : project.pages) {
        used.insert(ref.sourceDocId);
        (void)findSource(project, ref.sourceDocId);
        (void)normalizedRotation(ref.rotation);
    }
    if (used.size() == 1) {
        return copySingleSource(project, findSource(project, *used.begin()), output, options, ctx);
    }
    return copyMultiSource(project, output, options, ctx);
}

} // namespace pdfcore
