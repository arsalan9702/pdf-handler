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

// Replaces the document outline with flat entries, one per bookmark.
void applyBookmarks(QPDF& pdf, const std::vector<Bookmark>& bookmarks) {
    if (bookmarks.empty()) {
        return;
    }
    const auto& pages = pdf.getAllPages();
    auto outlines = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    outlines.replaceKey("/Type", QPDFObjectHandle::newName("/Outlines"));
    QPDFObjectHandle first;
    QPDFObjectHandle prev;
    for (const auto& bm : bookmarks) {
        if (bm.outputPageIndex < 0 || std::cmp_greater_equal(bm.outputPageIndex, pages.size())) {
            throw PdfError(ErrorKind::InvalidArgument,
                           "Bookmark \"" + bm.title + "\" points past the last page");
        }
        auto item = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
        item.replaceKey("/Title", QPDFObjectHandle::newUnicodeString(bm.title));
        item.replaceKey("/Parent", outlines);
        auto dest = QPDFObjectHandle::newArray();
        dest.appendItem(pages[static_cast<std::size_t>(bm.outputPageIndex)]);
        dest.appendItem(QPDFObjectHandle::newName("/Fit"));
        item.replaceKey("/Dest", dest);
        if (prev.isInitialized()) {
            prev.replaceKey("/Next", item);
            item.replaceKey("/Prev", prev);
        } else {
            first = item;
        }
        prev = item;
    }
    outlines.replaceKey("/First", first);
    outlines.replaceKey("/Last", prev);
    outlines.replaceKey("/Count",
                        QPDFObjectHandle::newInteger(static_cast<long long>(bookmarks.size())));
    pdf.getRoot().replaceKey("/Outlines", outlines);
}

OpResult copySingleSource(const Project& project, const SourceDoc& source,
                          const std::filesystem::path& output, const WriteOptions& options,
                          const JobContext& ctx) {
    const int n = static_cast<int>(project.pages.size());
    OpResult result;

    // Open once ourselves for precise errors (password, damage) and bounds checking.
    auto opened = openDocument(source.path, source.password);
    appendWarnings(result.warnings, opened.warnings);
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
        appendWarnings(result.warnings, takeWarnings(*pdf));
        applyBookmarks(*pdf, project.bookmarks);
        ctx.report(n, 2 * n);

        auto validation =
            writeAndValidate(*pdf, out, options, n, reopenPasswordFor(options, source.password),
                             ctx.slice(n, n, 2 * n));
        appendWarnings(result.warnings, takeWarnings(*pdf));
        appendWarnings(result.warnings, validation.warnings);
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

} // namespace

struct SourceCache::Impl {
    std::map<DocId, OpenSource> sources;
};

SourceCache::SourceCache() : impl(std::make_unique<Impl>()) {}
SourceCache::~SourceCache() = default;

OpResult copyPages(SourceCache& cache, const Project& project, const std::filesystem::path& output,
                   const WriteOptions& requested, const JobContext& ctx) {
    if (project.pages.empty()) {
        throw PdfError(ErrorKind::InvalidArgument, "No pages selected");
    }
    const int n = static_cast<int>(project.pages.size());
    OpResult result;
    std::set<DocId> used;
    for (const auto& ref : project.pages) {
        used.insert(ref.sourceDocId);
        (void)normalizedRotation(ref.rotation);
    }

    WriteOptions options = requested;
    QPDF* encryptionSource = nullptr;
    std::string reopenPassword;

    AtomicOutput out(output);
    {
        // Every source stays open until write() returns (QPDF copies lazily).
        auto& sources = cache.impl->sources;
        for (DocId id : used) {
            if (sources.contains(id)) {
                continue;
            }
            const auto& src = findSource(project, id);
            OpenSource os;
            os.doc = openDocument(src.path, src.password);
            appendWarnings(result.warnings, os.doc.warnings);
            os.pages = QPDFPageDocumentHelper(*os.doc.pdf).getAllPages();
            os.forms = std::make_unique<QPDFAcroFormDocumentHelper>(*os.doc.pdf);
            sources.emplace(id, std::move(os));
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

            if (used.size() == 1) {
                // One source: keep its title/author and (if asked) its protection.
                auto& src = sources.at(*used.begin());
                auto info = src.doc.pdf->getTrailer().getKey("/Info");
                if (info.isDictionary() && info.isIndirect()) {
                    pdf->getTrailer().replaceKey("/Info", pdf->copyForeignObject(info));
                }
                if (options.encryptionMode == EncryptionMode::Preserve && src.doc.encrypted) {
                    encryptionSource = src.doc.pdf.get();
                    reopenPassword = findSource(project, *used.begin()).password;
                }
            }
        } catch (...) {
            rethrowAsPdfError("Copying pages", false);
        }
        applyBookmarks(*pdf, project.bookmarks);

        if (options.encryptionMode == EncryptionMode::Preserve && encryptionSource == nullptr) {
            options.encryptionMode = EncryptionMode::Remove; // a fresh document has none
        }
        if (encryptionSource == nullptr) {
            reopenPassword = reopenPasswordFor(options, {});
        }
        auto validation = writeAndValidate(*pdf, out, options, n, reopenPassword,
                                           ctx.slice(n, n, 2 * n), encryptionSource);
        appendWarnings(result.warnings, takeWarnings(*pdf));
        for (DocId id : used) {
            appendWarnings(result.warnings, takeWarnings(*sources.at(id).doc.pdf));
        }
        appendWarnings(result.warnings, validation.warnings);
    }
    // The cache may still hold the sources open; callers writing over a source must release
    // the cache before committing, so only commit when the target is not one of them.
    for (DocId id : used) {
        std::error_code ec;
        if (std::filesystem::equivalent(findSource(project, id).path, output, ec)) {
            cache.impl->sources.erase(id);
        }
    }
    out.commit();
    result.outputs.push_back(output);
    return result;
}

namespace {

OpResult copyMultiSource(const Project& project, const std::filesystem::path& output,
                         const WriteOptions& options, const JobContext& ctx) {
    SourceCache cache; // closed before return; writePages callers never reuse sources
    auto out = copyPages(cache, project, output, options, ctx);
    return out;
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
