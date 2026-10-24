// End-to-end checks of the app layer (models, undo, PdfTools + JobRunner) without QML.

#include "TestUtil.hpp"
#include "core/PageCopy.hpp"
#include "models/DocumentListModel.hpp"
#include "models/ImageListModel.hpp"
#include "models/ProjectPageModel.hpp"
#include "services/JobRunner.hpp"
#include "services/PdfTools.hpp"
#include "services/ThumbnailProvider.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QImage>
#include <QSignalSpy>
#include <QUrl>
#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

using testutil::fixture;
using testutil::markers;
using testutil::qpdfCheckPasses;
using testutil::TempDir;
using V = std::vector<int>;

namespace {

QUrl url(const std::filesystem::path& p) {
    return QUrl::fromLocalFile(QString::fromStdU16String(p.u16string()));
}

// Spins the event loop until `done()` or a timeout.
template <typename F> bool waitFor(F done, int ms = 20000) {
    QElapsedTimer t;
    t.start();
    while (!done() && t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return done();
}

struct JobResult {
    bool ok = false;
    QStringList outputs;
    QStringList warnings;
    QString summary;
    QString error;
};

// Starts a job via `start` and waits for Jobs.finished / Jobs.failed.
template <typename F> JobResult runJob(F start) {
    JobResult r;
    bool done = false;
    auto* jobs = JobRunner::instance();
    auto c1 = QObject::connect(jobs, &JobRunner::finished,
                               [&](const QString&, const QStringList& outputs,
                                   const QStringList& warnings, const QString& summary) {
                                   r = {true, outputs, warnings, summary, {}};
                                   done = true;
                               });
    auto c2 = QObject::connect(jobs, &JobRunner::failed,
                               [&](const QString&, const QString& message, bool) {
                                   r.error = message;
                                   done = true;
                               });
    const bool started = start();
    if (started) {
        waitFor([&] { return done; }, 60000);
    } else {
        r.error = QStringLiteral("not started");
    }
    QObject::disconnect(c1);
    QObject::disconnect(c2);
    return r;
}

bool allReady(DocumentListModel& docs) {
    return waitFor([&] { return docs.allReady(); });
}

} // namespace

TEST_CASE("DocumentList: loading, passwords and failures") {
    DocumentListModel docs;
    docs.addUrls({url(fixture("ten.pdf")), url(fixture("encrypted_user.pdf")),
                  url(fixture("not_a_pdf.pdf"))});
    REQUIRE(docs.rowCount() == 3);
    REQUIRE(waitFor([&] {
        return docs.entries()[1].status != DocumentListModel::Loading &&
               docs.entries()[2].status != DocumentListModel::Loading &&
               docs.entries()[0].status != DocumentListModel::Loading;
    }));
    CHECK(docs.entries()[0].status == DocumentListModel::Ready);
    CHECK(docs.entries()[0].pageCount == 10);
    CHECK(docs.entries()[1].status == DocumentListModel::NeedsPassword);
    CHECK(docs.entries()[2].status == DocumentListModel::Failed);
    CHECK_FALSE(docs.allReady());

    docs.unlock(1, QStringLiteral("nope"));
    REQUIRE(waitFor([&] { return docs.entries()[1].status == DocumentListModel::WrongPassword; }));
    docs.unlock(1, QStringLiteral("user"));
    REQUIRE(waitFor([&] { return docs.entries()[1].status == DocumentListModel::Ready; }));
    docs.remove(2);
    CHECK(docs.allReady());

    CHECK(docs.rangeError(0, QStringLiteral("1-3,8-")).isEmpty());
    CHECK_FALSE(docs.rangeError(0, QStringLiteral("11")).isEmpty());

    docs.setMaxDocuments(1);
    docs.clear();
    docs.addUrls({url(fixture("three.pdf"))});
    docs.addUrls({url(fixture("single.pdf"))}); // replaces, does not append
    CHECK(docs.rowCount() == 1);
    CHECK(docs.path(0).endsWith(QStringLiteral("single.pdf")));
}

TEST_CASE("PageProject: order, edits, undo/redo, export") {
    TempDir tmp;
    DocumentListModel docs;
    ProjectPageModel project;
    project.setDocuments(&docs);
    // ten.pdf is larger and finishes loading later; pages must still follow list order.
    docs.addUrls({url(fixture("ten.pdf")), url(fixture("three.pdf"))});
    REQUIRE(allReady(docs));
    REQUIRE(waitFor([&] { return project.rowCount() == 13; }));
    CHECK_FALSE(project.canUndo()); // initial files are not an edit
    CHECK(project.data(project.index(0), ProjectPageModel::PageNumberRole).toInt() == 1);
    CHECK(project.data(project.index(10), ProjectPageModel::DocIndexRole).toInt() == 1);

    project.select(0, false, false);
    project.select(2, false, true); // shift-extend: 0..2
    CHECK(project.selectedCount() == 3);
    project.rotate(-1, 90);
    CHECK(project.data(project.index(1), ProjectPageModel::RotationRole).toInt() == 90);
    project.remove(-1);
    CHECK(project.rowCount() == 10);
    CHECK(project.selectedCount() == 0);
    project.moveTo(9, 0); // last page (three.pdf p3) to the front
    project.duplicate(0);
    CHECK(project.rowCount() == 11);
    project.undo();
    project.undo();
    CHECK(project.rowCount() == 10);
    project.redo();
    CHECK(project.canRedo());
    CHECK(project.data(project.index(0), ProjectPageModel::DocIndexRole).toInt() == 1);

    // Pages now: three p3, ten p4..p10, three p1, three p2.
    const auto out = tmp / "organized.pdf";
    auto r =
        runJob([&] { return PdfTools::create(nullptr, nullptr)->exportPages(&project, url(out)); });
    REQUIRE(r.ok);
    CHECK(markers(out) == V{3, 4, 5, 6, 7, 8, 9, 10, 1, 2});
    CHECK(qpdfCheckPasses(out));
    CHECK_FALSE(project.modified());

    project.select(0, false, false);
    project.select(9, true, false);
    const auto sel = tmp / "selection.pdf";
    r = runJob(
        [&] { return PdfTools::create(nullptr, nullptr)->exportPages(&project, url(sel), true); });
    REQUIRE(r.ok);
    CHECK(markers(sel) == V{3, 2});

    docs.remove(1); // dropping a file removes its pages and the history referencing them
    CHECK(project.rowCount() == 7);
    CHECK_FALSE(project.canUndo());
}

TEST_CASE("Tools: merge, split, protect, unlock, properties") {
    TempDir tmp;
    auto* tools = PdfTools::create(nullptr, nullptr);
    DocumentListModel docs;
    docs.addUrls({url(fixture("three.pdf")), url(fixture("ten.pdf"))});
    REQUIRE(allReady(docs));

    docs.setRange(1, QStringLiteral("9-"));
    auto r = runJob([&] { return tools->merge(&docs, url(tmp / "merged.pdf"), true); });
    REQUIRE(r.ok);
    CHECK(markers(tmp / "merged.pdf") == V{1, 2, 3, 9, 10});

    QSignalSpy invalid(tools, &PdfTools::invalidInput);
    docs.setRange(1, QStringLiteral("99"));
    CHECK_FALSE(tools->merge(&docs, url(tmp / "x.pdf"), false));
    CHECK(invalid.count() == 1);
    docs.setRange(1, {});

    auto preview =
        tools->splitPreview(&docs, 1, PdfTools::EveryN, 4, {}, QStringLiteral("{name}-{n}"));
    CHECK(preview.value(QStringLiteral("names")).toStringList().size() == 3);
    r = runJob([&] {
        return tools->split(&docs, 1, PdfTools::ByRanges, 1, QStringLiteral("1-2\n5"),
                            QStringLiteral("part-{n}"), url(tmp.path()));
    });
    REQUIRE(r.ok);
    CHECK(markers(tmp / "part-2.pdf") == V{5});

    const auto locked = tmp / "locked.pdf";
    DocumentListModel one;
    one.addUrls({url(fixture("three.pdf"))});
    REQUIRE(allReady(one));
    r = runJob([&] {
        return tools->protect(&one, QStringLiteral("pw"), {}, false, true, true, url(locked));
    });
    REQUIRE(r.ok);
    CHECK(markers(locked, "pw") == V{1, 2, 3});

    DocumentListModel lockedDocs;
    lockedDocs.addUrls({url(locked)});
    REQUIRE(waitFor(
        [&] { return lockedDocs.entries()[0].status == DocumentListModel::NeedsPassword; }));
    lockedDocs.unlock(0, QStringLiteral("pw"));
    REQUIRE(allReady(lockedDocs));
    r = runJob([&] { return tools->unlock(&lockedDocs, url(tmp / "open.pdf")); });
    REQUIRE(r.ok);
    CHECK_FALSE(pdfcore::openDocument(tmp / "open.pdf").encrypted);

    QVariantMap fields{{QStringLiteral("title"), QStringLiteral("Hello")},
                       {QStringLiteral("author"), QStringLiteral("Me")}};
    r = runJob([&] { return tools->writeMetadata(&one, 0, fields, url(tmp / "meta.pdf")); });
    REQUIRE(r.ok);
    DocumentListModel metaDoc;
    metaDoc.addUrls({url(tmp / "meta.pdf")});
    REQUIRE(allReady(metaDoc));
    CHECK(tools->readMetadata(&metaDoc, 0).value(QStringLiteral("title")).toString() ==
          QStringLiteral("Hello"));
}

TEST_CASE("Tools: compress presets") {
    TempDir tmp;
    auto* tools = PdfTools::create(nullptr, nullptr);
    DocumentListModel docs;
    docs.addUrls({url(fixture("images.pdf"))});
    REQUIRE(allReady(docs));
    const auto before = std::filesystem::file_size(fixture("images.pdf"));
    for (int preset : {PdfTools::Lossless, PdfTools::Balanced, PdfTools::Small}) {
        const auto out = tmp / ("c" + std::to_string(preset) + ".pdf");
        auto r = runJob([&] { return tools->compress(&docs, preset, url(out)); });
        REQUIRE(r.ok);
        CHECK(markers(out) == V{1, 2});
        CHECK(qpdfCheckPasses(out));
        if (preset != PdfTools::Lossless) {
            CHECK(std::filesystem::file_size(out) < before / 2);
        }
    }

    // Several files go to a folder with unique names.
    DocumentListModel many;
    many.addUrls({url(fixture("images.pdf")), url(fixture("three.pdf"))});
    REQUIRE(allReady(many));
    auto r = runJob([&] { return tools->compress(&many, PdfTools::Balanced, url(tmp.path())); });
    REQUIRE(r.ok);
    CHECK(r.outputs.size() == 2);
    CHECK(r.outputs[0].endsWith(QStringLiteral("images-compressed.pdf")));
}

TEST_CASE("Tools: images to PDF and PDF to images") {
    TempDir tmp;
    auto* tools = PdfTools::create(nullptr, nullptr);

    // A PNG with transparency and an EXIF-free JPEG.
    QImage png(300, 200, QImage::Format_ARGB32);
    png.fill(QColor(255, 0, 0, 128));
    const auto pngPath = tmp / "red.png";
    REQUIRE(png.save(QString::fromStdU16String(pngPath.u16string())));

    ImageListModel images;
    CHECK(images.addUrls({url(fixture("photo.jpg")), url(pngPath), url(fixture("three.pdf"))}) ==
          1);
    REQUIRE(images.rowCount() == 2);
    auto r = runJob([&] { return tools->imagesToPdf(&images, 1, 0, 10, url(tmp / "imgs.pdf")); });
    REQUIRE(r.ok);
    CHECK(pdfcore::openDocument(tmp / "imgs.pdf").pageCount == 2);
    CHECK(qpdfCheckPasses(tmp / "imgs.pdf"));

    DocumentListModel docs;
    docs.addUrls({url(fixture("three.pdf"))});
    REQUIRE(allReady(docs));
    const auto outDir = tmp / "pngs";
    std::filesystem::create_directories(outDir);
    r = runJob([&] {
        return tools->pdfToImages(&docs, 0, QStringLiteral("1,3"), 72, QStringLiteral("png"), 90,
                                  url(outDir));
    });
    REQUIRE(r.ok);
    REQUIRE(r.outputs.size() == 2);
    QImage page(r.outputs[1]);
    CHECK(page.width() == 612);
    CHECK(page.height() == 792);
    CHECK(r.outputs[1].endsWith(QStringLiteral("three-3.png")));

    // Refuses to overwrite existing images.
    r = runJob([&] {
        return tools->pdfToImages(&docs, 0, QStringLiteral("1"), 72, QStringLiteral("png"), 90,
                                  url(outDir));
    });
    CHECK_FALSE(r.ok);
    CHECK(r.error.contains(QStringLiteral("already exists")));
}

TEST_CASE("Jobs: cancel leaves the target untouched") {
    TempDir tmp;
    const auto out = tmp / "existing.pdf";
    std::filesystem::copy_file(fixture("three.pdf"), out);
    const auto original = testutil::readFile(out);
    auto* jobs = JobRunner::instance();
    auto r = runJob([&] {
        return jobs->start(QStringLiteral("slow"), [out](const pdfcore::JobContext& ctx) {
            pdfcore::Project p;
            p.sources.push_back({1, fixture("ten.pdf"), {}});
            for (int i = 0; i < 10; ++i) {
                p.pages.push_back({1, i, 0});
            }
            // Cancel from inside the first progress callback.
            const_cast<pdfcore::CancelToken*>(ctx.cancel)->cancel();
            pdfcore::writePages(p, out, {}, ctx);
            return JobOutcome{};
        });
    });
    CHECK_FALSE(r.ok);
    CHECK(r.error == QStringLiteral("Cancelled"));
    CHECK(testutil::readFile(out) == original);
    CHECK(tmp.files().size() == 1);
}

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    ThumbnailProvider thumbnails; // models register documents with it
    return Catch::Session().run(argc, argv);
}
