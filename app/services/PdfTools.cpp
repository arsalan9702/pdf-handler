#include "services/PdfTools.hpp"

#include "core/Document.hpp"
#include "core/PageCopy.hpp"
#include "core/ops/Compress.hpp"
#include "core/ops/Encrypt.hpp"
#include "core/ops/Extract.hpp"
#include "core/ops/ImagesToPdf.hpp"
#include "core/ops/Merge.hpp"
#include "core/ops/Metadata.hpp"
#include "core/ops/Split.hpp"
#include "models/DocumentListModel.hpp"
#include "models/ImageListModel.hpp"
#include "models/ProjectPageModel.hpp"
#include "services/JobRunner.hpp"
#include "services/QImageRecoder.hpp"

#include <QBuffer>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QJSEngine>
#include <QLocale>
#include <QPainter>
#include <QPdfDocument>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <array>
#include <cmath>
#include <cstring>

namespace fs = std::filesystem;
using namespace pdfcore;

namespace {

fs::path toPath(const QString& path) {
    return {path.toStdU16String()};
}

QString fromPath(const fs::path& path) {
    return QString::fromStdU16String(path.u16string());
}

QString localPath(const QUrl& url) {
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

QStringList toQt(const std::vector<std::string>& v) {
    QStringList out;
    for (const auto& s : v) {
        out << QString::fromUtf8(s);
    }
    return out;
}

JobOutcome outcomeOf(const OpResult& r) {
    JobOutcome o;
    for (const auto& p : r.outputs) {
        o.outputs << fromPath(p);
    }
    o.warnings = toQt(r.warnings);
    return o;
}

// "<folder>/<stem><suffix>.<ext>", adding " (2)", " (3)"... if that file exists.
QString uniquePath(const QString& folder, const QString& stem, const QString& ext) {
    QString candidate = QDir(folder).filePath(stem + ext);
    for (int n = 2; QFileInfo::exists(candidate); ++n) {
        candidate = QDir(folder).filePath(QStringLiteral("%1 (%2)%3").arg(stem).arg(n).arg(ext));
    }
    return candidate;
}

QImage onWhite(const QImage& image) {
    QImage out(image.size(), QImage::Format_RGB32);
    out.setDotsPerMeterX(image.dotsPerMeterX());
    out.setDotsPerMeterY(image.dotsPerMeterY());
    out.fill(Qt::white);
    QPainter painter(&out);
    painter.drawImage(0, 0, image);
    return out;
}

// Turns an image file into core input: JPEGs pass through untouched when possible; everything
// else is decoded (EXIF rotation applied, transparency flattened onto white).
ImageInput prepareImage(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw PdfError(ErrorKind::Io, "Cannot open " + path.toStdString());
    }
    const QByteArray bytes = file.readAll();
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QByteArray format = reader.format();
    const bool isJpeg = format == "jpeg" || format == "jpg";
    if (isJpeg && reader.transformation() == QImageIOHandler::TransformationNone) {
        if (auto info =
                parseJpegHeader({bytes.constData(), static_cast<std::size_t>(bytes.size())});
            info && (info->components == 1 || info->components == 3)) {
            return {ImageInput::Kind::Jpeg, bytes.toStdString()};
        }
    }
    QImage image = reader.read();
    if (image.isNull()) {
        throw PdfError(ErrorKind::InvalidArgument, QFileInfo(path).fileName().toStdString() + ": " +
                                                       reader.errorString().toStdString());
    }
    if (image.hasAlphaChannel()) {
        image = onWhite(image);
    }
    // 2835 dots/m (72 dpi) is QImage's default, i.e. "unknown"; use the core default then.
    const double dpi = image.dotsPerMeterX() > 0 && image.dotsPerMeterX() != 2835
                           ? image.dotsPerMeterX() * 0.0254
                           : 0.0;
    if (isJpeg) {
        // Rotated or CMYK JPEG: re-encode at high quality instead of storing raw pixels.
        QByteArray jpeg;
        QBuffer buffer(&jpeg);
        buffer.open(QIODevice::WriteOnly);
        QImageWriter writer(&buffer, "jpeg");
        writer.setQuality(92);
        if (writer.write(image.convertToFormat(QImage::Format_RGB888))) {
            ImageInput in{ImageInput::Kind::Jpeg, jpeg.toStdString()};
            in.dpi = dpi;
            return in;
        }
    }
    const bool gray = image.allGray();
    const QImage px =
        image.convertToFormat(gray ? QImage::Format_Grayscale8 : QImage::Format_RGB888);
    const int components = gray ? 1 : 3;
    const auto rowBytes = static_cast<std::size_t>(px.width()) * components;
    ImageInput in;
    in.kind = ImageInput::Kind::Pixels;
    in.width = px.width();
    in.height = px.height();
    in.components = components;
    in.dpi = dpi;
    in.data.resize(rowBytes * static_cast<std::size_t>(px.height()));
    for (int y = 0; y < px.height(); ++y) {
        std::memcpy(in.data.data() + rowBytes * static_cast<std::size_t>(y), px.constScanLine(y),
                    rowBytes);
    }
    return in;
}

} // namespace

PdfTools* PdfTools::create(QQmlEngine*, QJSEngine*) {
    static auto* tools = new PdfTools(QCoreApplication::instance());
    QJSEngine::setObjectOwnership(tools, QJSEngine::CppOwnership);
    return tools;
}

bool PdfTools::reject(const QString& message) {
    emit invalidInput(message);
    return false;
}

bool PdfTools::requireReady(DocumentListModel* docs, int minimum) {
    if (docs == nullptr || docs->rowCount() < minimum) {
        return reject(minimum > 1 ? tr("Add at least %1 PDF files").arg(minimum)
                                  : tr("Add a PDF file first"));
    }
    if (!docs->allReady()) {
        return reject(tr("Some files are still loading, locked or unreadable"));
    }
    if (JobRunner::instance()->busy()) {
        return reject(tr("Another task is still running"));
    }
    return true;
}

bool PdfTools::exportPages(ProjectPageModel* project, const QUrl& output, bool selectedOnly) {
    if (project == nullptr || project->rowCount() == 0) {
        return reject(tr("There are no pages to save"));
    }
    if (selectedOnly && project->selectedCount() == 0) {
        return reject(tr("Select the pages to save first"));
    }
    if (!requireReady(project->documents())) {
        return false;
    }
    auto p = project->project(selectedOnly);
    const auto out = toPath(localPath(output));
    QPointer<ProjectPageModel> model(project);
    connect(
        JobRunner::instance(), &JobRunner::finished, project,
        [model, selectedOnly](const QString& title) {
            if (model && !selectedOnly && title == tr("Saving pages")) {
                model->markSaved();
            }
        },
        Qt::SingleShotConnection);
    return JobRunner::instance()->start(tr("Saving pages"), [p, out](const JobContext& ctx) {
        return outcomeOf(writePages(p, out, {}, ctx));
    });
}

bool PdfTools::extract(DocumentListModel* docs, int row, const QString& range, const QUrl& output) {
    if (!requireReady(docs)) {
        return false;
    }
    if (auto error = docs->rangeError(row, range); !error.isEmpty()) {
        return reject(error);
    }
    auto src = docs->source(row);
    const auto out = toPath(localPath(output));
    const auto r = range.toStdString();
    return JobRunner::instance()->start(tr("Extracting pages"),
                                        [src, out, r](const JobContext& ctx) {
                                            return outcomeOf(extractPages(src, r, out, {}, ctx));
                                        });
}

bool PdfTools::merge(DocumentListModel* docs, const QUrl& output, bool bookmarkPerFile) {
    if (!requireReady(docs, 2)) {
        return false;
    }
    std::vector<MergeInput> inputs;
    for (int i = 0; i < docs->rowCount(); ++i) {
        const auto& e = docs->entries()[static_cast<std::size_t>(i)];
        if (auto error = docs->rangeError(i, e.range); !error.isEmpty()) {
            return reject(QFileInfo(e.path).fileName() + ": " + error);
        }
        inputs.push_back({docs->source(i), e.range.toStdString()});
    }
    MergeOptions options;
    options.bookmarkPerFile = bookmarkPerFile;
    const auto out = toPath(localPath(output));
    return JobRunner::instance()->start(tr("Merging %n files", "", docs->rowCount()),
                                        [inputs, options, out](const JobContext& ctx) {
                                            return outcomeOf(
                                                mergeDocuments(inputs, out, options, ctx));
                                        });
}

namespace {

SplitOptions splitOptions(int mode, int everyN, const QString& ranges,
                          const QString& nameTemplate) {
    SplitOptions o;
    o.mode = mode == PdfTools::EveryN     ? SplitMode::EveryNPages
             : mode == PdfTools::ByRanges ? SplitMode::Ranges
                                          : SplitMode::EachPage;
    o.everyN = std::max(1, everyN);
    for (const auto& part :
         ranges.split(QRegularExpression(QStringLiteral("[;\\n]")), Qt::SkipEmptyParts)) {
        if (!part.trimmed().isEmpty()) {
            o.ranges.push_back(part.trimmed().toStdString());
        }
    }
    if (!nameTemplate.trimmed().isEmpty()) {
        o.nameTemplate = nameTemplate.trimmed().toStdString();
    }
    return o;
}

} // namespace

QVariantMap PdfTools::splitPreview(DocumentListModel* docs, int row, int mode, int everyN,
                                   const QString& ranges, const QString& nameTemplate) {
    if (docs == nullptr || row < 0 || row >= docs->rowCount() ||
        docs->entries()[static_cast<std::size_t>(row)].status != DocumentListModel::Ready) {
        return {};
    }
    try {
        QStringList names;
        for (const auto& p :
             planSplit(docs->source(row), {}, splitOptions(mode, everyN, ranges, nameTemplate))) {
            names << fromPath(p.filename());
        }
        return {{QStringLiteral("names"), names}};
    } catch (const std::exception& e) {
        return {{QStringLiteral("error"), QString::fromUtf8(e.what())}};
    }
}

bool PdfTools::split(DocumentListModel* docs, int row, int mode, int everyN, const QString& ranges,
                     const QString& nameTemplate, const QUrl& folder) {
    if (!requireReady(docs)) {
        return false;
    }
    auto preview = splitPreview(docs, row, mode, everyN, ranges, nameTemplate);
    if (preview.contains(QStringLiteral("error"))) {
        return reject(preview.value(QStringLiteral("error")).toString());
    }
    auto src = docs->source(row);
    auto options = splitOptions(mode, everyN, ranges, nameTemplate);
    const auto dir = toPath(localPath(folder));
    return JobRunner::instance()->start(
        tr("Splitting"), [src, options, dir](const JobContext& ctx) {
            auto o = outcomeOf(splitDocument(src, dir, options, ctx));
            o.summary = tr("Created %n files", "", o.outputs.size());
            return o;
        });
}

bool PdfTools::compress(DocumentListModel* docs, int preset, const QUrl& target) {
    if (!requireReady(docs)) {
        return false;
    }
    struct Task {
        SourceDoc src;
        fs::path out;
    };
    std::vector<Task> tasks;
    const QString targetPath = localPath(target);
    for (int i = 0; i < docs->rowCount(); ++i) {
        const QFileInfo info(docs->path(i));
        const QString out =
            docs->rowCount() == 1
                ? targetPath
                : uniquePath(targetPath, info.completeBaseName() + QStringLiteral("-compressed"),
                             QStringLiteral(".pdf"));
        tasks.push_back({docs->source(i), toPath(out)});
    }
    return JobRunner::instance()->start(tr("Compressing"), [this, tasks,
                                                            preset](const JobContext& ctx) {
        const int n = static_cast<int>(tasks.size());
        JobOutcome outcome;
        std::uintmax_t before = 0;
        std::uintmax_t after = 0;
        for (int i = 0; i < n; ++i) {
            // Quality / longest edge in pixels for the lossy presets.
            QImageRecoder balanced(75, 2400);
            QImageRecoder small(55, 1600);
            CompressOptions options;
            options.recoder = preset == Balanced ? &balanced
                              : preset == Small  ? static_cast<ImageRecoder*>(&small)
                                                 : nullptr;
            const auto& t = tasks[static_cast<std::size_t>(i)];
            auto report = compressDocument(t.src, t.out, options, ctx.slice(i, 1, n));
            auto o = outcomeOf(report.result);
            outcome.outputs << o.outputs;
            outcome.warnings << o.warnings;
            before += report.bytesBefore;
            after += report.bytesAfter;
        }
        const double saved = before > 0 ? 100.0 * (1.0 - static_cast<double>(after) / before) : 0.0;
        outcome.summary = tr("%1 → %2 (%3% smaller)")
                              .arg(formatBytes(static_cast<qint64>(before)),
                                   formatBytes(static_cast<qint64>(after)))
                              .arg(std::max(0.0, saved), 0, 'f', 0);
        return outcome;
    });
}

bool PdfTools::protect(DocumentListModel* docs, const QString& userPassword,
                       const QString& ownerPassword, bool allowPrint, bool allowCopy,
                       bool allowModify, const QUrl& target) {
    if (!requireReady(docs)) {
        return false;
    }
    if (userPassword.isEmpty() && ownerPassword.isEmpty()) {
        return reject(tr("Enter a password"));
    }
    Encryption enc;
    enc.userPassword = userPassword.toStdString();
    enc.ownerPassword = ownerPassword.toStdString();
    enc.permissions.print = allowPrint;
    enc.permissions.highQualityPrint = allowPrint;
    enc.permissions.extract = allowCopy;
    enc.permissions.modify = allowModify;
    enc.permissions.assemble = allowModify;
    enc.permissions.annotate = allowModify;
    std::vector<std::pair<SourceDoc, fs::path>> tasks;
    const QString targetPath = localPath(target);
    for (int i = 0; i < docs->rowCount(); ++i) {
        const QFileInfo info(docs->path(i));
        const QString out =
            docs->rowCount() == 1
                ? targetPath
                : uniquePath(targetPath, info.completeBaseName() + QStringLiteral("-protected"),
                             QStringLiteral(".pdf"));
        tasks.emplace_back(docs->source(i), toPath(out));
    }
    return JobRunner::instance()->start(tr("Protecting"), [tasks, enc](const JobContext& ctx) {
        JobOutcome outcome;
        const int n = static_cast<int>(tasks.size());
        for (int i = 0; i < n; ++i) {
            const auto& [src, out] = tasks[static_cast<std::size_t>(i)];
            auto o = outcomeOf(encryptDocument(src, enc, out, ctx.slice(i, 1, n)));
            outcome.outputs << o.outputs;
            outcome.warnings << o.warnings;
        }
        return outcome;
    });
}

bool PdfTools::unlock(DocumentListModel* docs, const QUrl& target) {
    if (!requireReady(docs)) {
        return false;
    }
    std::vector<std::pair<SourceDoc, fs::path>> tasks;
    const QString targetPath = localPath(target);
    for (int i = 0; i < docs->rowCount(); ++i) {
        const QFileInfo info(docs->path(i));
        const QString out =
            docs->rowCount() == 1
                ? targetPath
                : uniquePath(targetPath, info.completeBaseName() + QStringLiteral("-unlocked"),
                             QStringLiteral(".pdf"));
        tasks.emplace_back(docs->source(i), toPath(out));
    }
    return JobRunner::instance()->start(tr("Removing protection"), [tasks](const JobContext& ctx) {
        JobOutcome outcome;
        const int n = static_cast<int>(tasks.size());
        for (int i = 0; i < n; ++i) {
            const auto& [src, out] = tasks[static_cast<std::size_t>(i)];
            auto o = outcomeOf(decryptDocument(src, out, ctx.slice(i, 1, n)));
            outcome.outputs << o.outputs;
            outcome.warnings << o.warnings;
        }
        return outcome;
    });
}

QVariantMap PdfTools::readMetadata(DocumentListModel* docs, int row) {
    if (docs == nullptr || row < 0 || row >= docs->rowCount() ||
        docs->entries()[static_cast<std::size_t>(row)].status != DocumentListModel::Ready) {
        return {};
    }
    try {
        const auto info = readDocumentInfo(docs->source(row));
        const auto& m = info.metadata;
        return {{QStringLiteral("title"), QString::fromUtf8(m.title)},
                {QStringLiteral("author"), QString::fromUtf8(m.author)},
                {QStringLiteral("subject"), QString::fromUtf8(m.subject)},
                {QStringLiteral("keywords"), QString::fromUtf8(m.keywords)},
                {QStringLiteral("creator"), QString::fromUtf8(m.creator)},
                {QStringLiteral("producer"), QString::fromUtf8(m.producer)},
                {QStringLiteral("created"), QString::fromUtf8(m.creationDate)},
                {QStringLiteral("modified"), QString::fromUtf8(m.modDate)},
                {QStringLiteral("pdfVersion"), QString::fromUtf8(info.pdfVersion)},
                {QStringLiteral("hasXmp"), info.hasXmpMetadata}};
    } catch (const std::exception& e) {
        emit invalidInput(QString::fromUtf8(e.what()));
        return {};
    }
}

bool PdfTools::writeMetadata(DocumentListModel* docs, int row, const QVariantMap& fields,
                             const QUrl& output) {
    if (!requireReady(docs)) {
        return false;
    }
    Metadata m;
    auto field = [&](const char* key) {
        return fields.value(QString::fromLatin1(key)).toString().trimmed().toStdString();
    };
    m.title = field("title");
    m.author = field("author");
    m.subject = field("subject");
    m.keywords = field("keywords");
    m.creator = field("creator");
    m.producer = field("producer");
    auto src = docs->source(row);
    const auto out = toPath(localPath(output));
    return JobRunner::instance()->start(
        tr("Saving properties"), [src, m, out](const JobContext& ctx) {
            return outcomeOf(pdfcore::writeMetadata(src, m, out, ctx));
        });
}

bool PdfTools::imagesToPdf(ImageListModel* images, int pageSize, int orientation, double marginMm,
                           const QUrl& output) {
    if (images == nullptr || images->rowCount() == 0) {
        return reject(tr("Add some images first"));
    }
    if (JobRunner::instance()->busy()) {
        return reject(tr("Another task is still running"));
    }
    const QStringList paths = images->paths();
    ImagesToPdfOptions options;
    options.pageSize = pageSize == 1   ? PageSize::A4
                       : pageSize == 2 ? PageSize::Letter
                                       : PageSize::FitImage;
    options.orientation = orientation == 1   ? Orientation::Portrait
                          : orientation == 2 ? Orientation::Landscape
                                             : Orientation::Auto;
    options.marginPoints = marginMm * 72.0 / 25.4;
    const auto out = toPath(localPath(output));
    return JobRunner::instance()->start(
        tr("Creating PDF from %n images", "", paths.size()),
        [paths, options, out](const JobContext& ctx) {
            const int n = static_cast<int>(paths.size());
            std::vector<ImageInput> inputs;
            for (int i = 0; i < n; ++i) {
                ctx.checkCancel();
                inputs.push_back(prepareImage(paths.at(i)));
                ctx.report(i + 1, 2 * n);
            }
            return outcomeOf(pdfcore::imagesToPdf(inputs, out, options, ctx.slice(n, n, 2 * n)));
        });
}

bool PdfTools::pdfToImages(DocumentListModel* docs, int row, const QString& range, int dpi,
                           const QString& format, int quality, const QUrl& folder) {
    if (!requireReady(docs)) {
        return false;
    }
    if (auto error = docs->rangeError(row, range); !error.isEmpty()) {
        return reject(error);
    }
    auto src = docs->source(row);
    const QString dir = localPath(folder);
    const QString ext = format.compare(QStringLiteral("jpeg"), Qt::CaseInsensitive) == 0
                            ? QStringLiteral("jpg")
                            : QStringLiteral("png");
    const auto r = range.toStdString();
    return JobRunner::instance()->start(
        tr("Exporting pages as images"), [src, dir, ext, dpi, quality, r](const JobContext& ctx) {
            const auto pages = resolveRange(src, r);
            const QString stem = QString::fromStdU16String(src.path.stem().u16string());
            const int width = static_cast<int>(QString::number(countPages(src)).size());
            QStringList targets;
            for (int page : pages) {
                const QString name = QStringLiteral("%1-%2.%3")
                                         .arg(stem)
                                         .arg(page + 1, width, 10, QLatin1Char('0'))
                                         .arg(ext);
                targets << QDir(dir).filePath(name);
                if (QFileInfo::exists(targets.back())) {
                    throw PdfError(ErrorKind::Io, "File already exists: " + name.toStdString() +
                                                      ". Choose an empty folder.");
                }
            }

            QPdfDocument doc;
            doc.setPassword(QString::fromStdString(src.password));
            if (doc.load(QString::fromStdU16String(src.path.u16string())) !=
                QPdfDocument::Error::None) {
                throw PdfError(ErrorKind::Damaged, "The PDF could not be rendered");
            }
            JobOutcome outcome;
            const int n = static_cast<int>(pages.size());
            for (int i = 0; i < n; ++i) {
                ctx.checkCancel();
                const int page = pages[static_cast<std::size_t>(i)];
                QSizeF px = doc.pagePointSize(page) * (dpi / 72.0);
                // Keep pathological page sizes from exhausting memory (~250 MP limit).
                const double maxPixels = 250e6;
                if (px.width() * px.height() > maxPixels) {
                    px *= std::sqrt(maxPixels / (px.width() * px.height()));
                }
                const QImage image = onWhite(doc.render(page, px.toSize().expandedTo({1, 1})));
                QSaveFile file(targets.at(i));
                if (!file.open(QIODevice::WriteOnly)) {
                    throw PdfError(ErrorKind::Io, file.errorString().toStdString());
                }
                QImageWriter writer(&file, ext == QLatin1String("jpg") ? "jpeg" : "png");
                if (ext == QLatin1String("jpg")) {
                    writer.setQuality(quality);
                }
                if (!writer.write(image) || !file.commit()) {
                    throw PdfError(ErrorKind::Io, writer.errorString().toStdString());
                }
                outcome.outputs << targets.at(i);
                ctx.report(i + 1, n);
            }
            outcome.summary = tr("Saved %n images", "", n);
            return outcome;
        });
}

QUrl PdfTools::suggestedFile(DocumentListModel* docs, int row, const QString& suffix) const {
    if (docs == nullptr || row < 0 || row >= docs->rowCount()) {
        // No source document: `suffix` is the whole name (e.g. "images").
        const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        return QUrl::fromLocalFile(uniquePath(
            docs.isEmpty() ? QDir::homePath() : docs,
            suffix.isEmpty() ? QStringLiteral("document") : suffix, QStringLiteral(".pdf")));
    }
    const QFileInfo info(docs->path(row));
    return QUrl::fromLocalFile(
        uniquePath(info.absolutePath(), info.completeBaseName() + suffix, QStringLiteral(".pdf")));
}

QUrl PdfTools::folderOf(DocumentListModel* docs, int row) const {
    if (docs == nullptr || row < 0 || row >= docs->rowCount()) {
        return QUrl::fromLocalFile(QDir::homePath());
    }
    return QUrl::fromLocalFile(QFileInfo(docs->path(row)).absolutePath());
}

void PdfTools::reveal(const QString& path) const {
    const QString native = QDir::toNativeSeparators(path);
#if defined(Q_OS_WIN)
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), native});
#elif defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), native});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void PdfTools::openFile(const QString& path) const {
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString PdfTools::formatBytes(qint64 bytes) const {
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}
