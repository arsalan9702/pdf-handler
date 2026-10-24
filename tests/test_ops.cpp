#include "TestUtil.hpp"
#include "core/ops/Compress.hpp"
#include "core/ops/Encrypt.hpp"
#include "core/ops/Extract.hpp"
#include "core/ops/ImagesToPdf.hpp"
#include "core/ops/Merge.hpp"
#include "core/ops/Metadata.hpp"
#include "core/ops/Split.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <qpdf/Pl_DCT.hh>
#include <qpdf/Pl_String.hh>
#include <qpdf/QPDFOutlineDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>

using namespace pdfcore;
using testutil::fixture;
using testutil::markers;
using testutil::qpdfCheckPasses;
using testutil::TempDir;
using V = std::vector<int>;

namespace {

SourceDoc src(const std::string& name, const std::string& password = {}) {
    return {0, fixture(name), password};
}

template <typename F> ErrorKind errorOf(F&& f) {
    try {
        f();
    } catch (const PdfError& e) {
        return e.kind();
    }
    FAIL("expected a PdfError");
    return ErrorKind::InvalidArgument;
}

std::string readBytes(const std::filesystem::path& p) {
    return testutil::readFile(p);
}

// Test double for the app's QImage recoder: re-encodes at the same size with libjpeg (via
// qpdf's Pl_DCT), optionally halving dimensions by dropping pixels.
class DctRecoder : public ImageRecoder {
  public:
    explicit DctRecoder(bool halve = false) : m_halve(halve) {}
    std::optional<EncodedJpeg> recode(const RawImage& img) override {
        ++calls;
        int w = img.width;
        int h = img.height;
        std::string px = img.pixels;
        if (m_halve && img.mayResize) {
            w /= 2;
            h /= 2;
            px.clear();
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    for (int c = 0; c < img.components; ++c) {
                        px.push_back(img.pixels[static_cast<std::size_t>(
                            ((y * 2) * img.width + x * 2) * img.components + c)]);
                    }
                }
            }
        }
        std::string out;
        Pl_String sink("jpeg", nullptr, out);
        Pl_DCT dct("dct", &sink, static_cast<JDIMENSION>(w), static_cast<JDIMENSION>(h),
                   img.components, img.components == 1 ? JCS_GRAYSCALE : JCS_RGB);
        dct.write(reinterpret_cast<const unsigned char*>(px.data()), px.size());
        dct.finish();
        return EncodedJpeg{w, h, out};
    }
    int calls = 0;

  private:
    bool m_halve;
};

// Filters of every image stream in the document, keyed by object id.
std::map<QPDFObjGen, std::string> imageFilters(const std::filesystem::path& path,
                                               const std::string& pw = {}) {
    auto doc = openDocument(path, pw);
    std::map<QPDFObjGen, std::string> out;
    for (auto& page : QPDFPageDocumentHelper(*doc.pdf).getAllPages()) {
        page.forEachImage(true, [&](QPDFObjectHandle& img, QPDFObjectHandle&, const std::string&) {
            auto f = img.getDict().getKey("/Filter");
            out[img.getObjGen()] = f.isName() ? f.getName() : "";
        });
    }
    return out;
}

int countFilter(const std::map<QPDFObjGen, std::string>& filters, const std::string& name) {
    int n = 0;
    for (const auto& [id, f] : filters) {
        n += f == name ? 1 : 0;
    }
    return n;
}

} // namespace

// ---------------------------------------------------------------- extract / delete / rotate

TEST_CASE("extractPages: range edge cases") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    SECTION("normal") {
        extractPages(src("ten.pdf"), "2-4,7", out);
        CHECK(markers(out) == V{2, 3, 4, 7});
    }
    SECTION("first") {
        extractPages(src("ten.pdf"), "1", out);
        CHECK(markers(out) == V{1});
    }
    SECTION("last") {
        extractPages(src("ten.pdf"), "z", out);
        CHECK(markers(out) == V{10});
    }
    SECTION("single-page document") {
        extractPages(src("single.pdf"), "1-z", out);
        CHECK(markers(out) == V{1});
    }
    SECTION("open-ended") {
        extractPages(src("ten.pdf"), "8-", out);
        CHECK(markers(out) == V{8, 9, 10});
    }
    SECTION("duplicates") {
        extractPages(src("ten.pdf"), "3,3,1", out);
        CHECK(markers(out) == V{3, 3, 1});
    }
    SECTION("empty range means all pages") {
        extractPages(src("three.pdf"), "", out);
        CHECK(markers(out) == V{1, 2, 3});
    }
    CHECK(qpdfCheckPasses(out));
}

TEST_CASE("extractPages: errors") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    CHECK(errorOf([&] { extractPages(src("ten.pdf"), "11", out); }) == ErrorKind::InvalidArgument);
    CHECK(errorOf([&] { extractPages(src("ten.pdf"), "0", out); }) == ErrorKind::InvalidArgument);
    CHECK(errorOf([&] { extractPages(src("not_a_pdf.pdf"), "1", out); }) == ErrorKind::NotAPdf);
    CHECK(errorOf([&] { extractPages(src("encrypted_user.pdf"), "1", out); }) ==
          ErrorKind::PasswordRequired);
    CHECK(tmp.files().empty());
}

TEST_CASE("extractPages: encrypted and damaged input") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    SECTION("encrypted") {
        extractPages(src("encrypted_user.pdf", "user"), "5,1", out);
        CHECK(markers(out, "user") == V{5, 1});
        CHECK(qpdfCheckPasses(out, "user"));
    }
    SECTION("damaged") {
        auto r = extractPages(src("broken_xref.pdf"), "9-", out);
        CHECK_FALSE(r.warnings.empty());
        CHECK(markers(out) == V{9, 10});
        CHECK(qpdfCheckPasses(out));
    }
}

TEST_CASE("deletePages") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    SECTION("normal") {
        deletePages(src("ten.pdf"), "2-9", out);
        CHECK(markers(out) == V{1, 10});
    }
    SECTION("first and last") {
        deletePages(src("ten.pdf"), "1,z", out);
        CHECK(markers(out) == V{2, 3, 4, 5, 6, 7, 8, 9});
    }
    SECTION("duplicates in the range are harmless") {
        deletePages(src("three.pdf"), "2,2", out);
        CHECK(markers(out) == V{1, 3});
    }
    SECTION("open-ended") {
        deletePages(src("ten.pdf"), "4-", out);
        CHECK(markers(out) == V{1, 2, 3});
    }
    SECTION("encrypted") {
        deletePages(src("encrypted_user.pdf", "owner"), "1", out);
        CHECK(markers(out, "user") == V{2, 3, 4, 5});
    }
    SECTION("damaged") {
        deletePages(src("broken_xref.pdf"), "1-5", out);
        CHECK(markers(out) == V{6, 7, 8, 9, 10});
    }
    CHECK(qpdfCheckPasses(
        out, std::filesystem::exists(out) && openDocument(out, "user").encrypted ? "user" : ""));
    CHECK(errorOf([&] { deletePages(src("single.pdf"), "1", tmp / "x.pdf"); }) ==
          ErrorKind::InvalidArgument);
    CHECK(errorOf([&] { deletePages(src("ten.pdf"), "", tmp / "x.pdf"); }) ==
          ErrorKind::InvalidArgument);
    CHECK(errorOf([&] { deletePages(src("ten.pdf"), "12", tmp / "x.pdf"); }) ==
          ErrorKind::InvalidArgument);
}

TEST_CASE("rotatePages") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    SECTION("selected pages only") {
        rotatePages(src("ten.pdf"), "1,z", 90, out);
        auto r = testutil::rotations(out);
        CHECK(r.front() == 90);
        CHECK(r.back() == 90);
        CHECK(r[4] == 0);
        CHECK(markers(out).size() == 10);
    }
    SECTION("all pages, counter-clockwise") {
        rotatePages(src("mixed.pdf"), "", -90, out);
        CHECK(testutil::rotations(out) == V{90, 90, 0, 180});
    }
    SECTION("open-ended") {
        rotatePages(src("ten.pdf"), "9-", 180, out);
        auto r = testutil::rotations(out);
        CHECK(r[8] == 180);
        CHECK(r[7] == 0);
    }
    SECTION("damaged input") {
        rotatePages(src("broken_xref.pdf"), "2", 270, out);
        CHECK(testutil::rotations(out)[1] == 270);
    }
    CHECK(qpdfCheckPasses(out));
    CHECK(errorOf([&] { rotatePages(src("ten.pdf"), "1", 45, tmp / "x.pdf"); }) ==
          ErrorKind::InvalidArgument);
}

TEST_CASE("rotatePages: encrypted input keeps its protection") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    rotatePages(src("encrypted_user.pdf", "user"), "2", 90, out);
    CHECK(openDocument(out, "user").encrypted);
    CHECK(qpdfCheckPasses(out, "user"));
}

// ---------------------------------------------------------------- merge

TEST_CASE("mergeDocuments") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    SECTION("whole documents in order") {
        mergeDocuments({{src("three.pdf"), ""}, {src("single.pdf"), ""}, {src("three.pdf"), ""}},
                       out);
        CHECK(markers(out) == V{1, 2, 3, 1, 1, 2, 3});
    }
    SECTION("ranges per input: first, last, open-ended, duplicates") {
        mergeDocuments({{src("ten.pdf"), "1,z"}, {src("three.pdf"), "2-"}, {src("ten.pdf"), "5,5"}},
                       out);
        CHECK(markers(out) == V{1, 10, 2, 3, 5, 5});
    }
    SECTION("single input") {
        mergeDocuments({{src("three.pdf"), ""}}, out);
        CHECK(markers(out) == V{1, 2, 3});
    }
    SECTION("encrypted and damaged inputs") {
        auto r = mergeDocuments({{src("encrypted_user.pdf", "user"), "1"},
                                 {src("broken_xref.pdf"), "10"},
                                 {src("encrypted_rc4.pdf", "owner"), "4"}},
                                out);
        CHECK(markers(out) == V{1, 10, 4});
        CHECK_FALSE(r.warnings.empty());
        CHECK_FALSE(openDocument(out).encrypted);
    }
    SECTION("bookmark per file") {
        MergeOptions opts;
        opts.bookmarkPerFile = true;
        mergeDocuments({{src("three.pdf"), ""}, {src("ten.pdf"), "1-2"}}, out, opts);
        auto doc = openDocument(out);
        QPDFOutlineDocumentHelper outlines(*doc.pdf);
        auto top = outlines.getTopLevelOutlines();
        REQUIRE(top.size() == 2);
        CHECK(top[0].getTitle() == "three");
        CHECK(top[1].getTitle() == "ten");
    }
    CHECK(qpdfCheckPasses(out));
}

TEST_CASE("mergeDocuments: errors leave nothing behind") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    CHECK(errorOf([&] { mergeDocuments({}, out); }) == ErrorKind::InvalidArgument);
    CHECK(errorOf([&] { mergeDocuments({{src("ten.pdf"), "1"}, {src("three.pdf"), "4"}}, out); }) ==
          ErrorKind::InvalidArgument);
    CHECK(errorOf([&] {
              mergeDocuments({{src("ten.pdf"), ""}, {src("encrypted_user.pdf"), ""}}, out);
          }) == ErrorKind::PasswordRequired);
    CHECK(errorOf([&] {
              mergeDocuments({{src("ten.pdf"), ""}, {src("not_a_pdf.pdf"), ""}}, out);
          }) == ErrorKind::NotAPdf);
    CHECK(tmp.files().empty());
}

// ---------------------------------------------------------------- split

TEST_CASE("splitDocument") {
    TempDir tmp;
    SECTION("each page") {
        auto r = splitDocument(src("three.pdf"), tmp.path());
        REQUIRE(r.outputs.size() == 3);
        CHECK(r.outputs[0].filename() == "three-1.pdf");
        CHECK(markers(r.outputs[2]) == V{3});
    }
    SECTION("every N pages, last part shorter, zero-padded names") {
        SplitOptions o;
        o.mode = SplitMode::EveryNPages;
        o.everyN = 1;
        auto r = splitDocument(src("ten.pdf"), tmp.path(), o);
        CHECK(r.outputs.front().filename() == "ten-01.pdf");
        o.everyN = 4;
        o.nameTemplate = "part {n} ({pages})";
        r = splitDocument(src("ten.pdf"), tmp.path(), o);
        REQUIRE(r.outputs.size() == 3);
        CHECK(r.outputs[2].filename() == "part 3 (9-10).pdf");
        CHECK(markers(r.outputs[0]) == V{1, 2, 3, 4});
        CHECK(markers(r.outputs[2]) == V{9, 10});
    }
    SECTION("ranges: first, last, open-ended, duplicates") {
        SplitOptions o;
        o.mode = SplitMode::Ranges;
        o.ranges = {"1", "z", "8-", "2,2"};
        auto r = splitDocument(src("ten.pdf"), tmp.path(), o);
        REQUIRE(r.outputs.size() == 4);
        CHECK(markers(r.outputs[0]) == V{1});
        CHECK(markers(r.outputs[1]) == V{10});
        CHECK(markers(r.outputs[2]) == V{8, 9, 10});
        CHECK(markers(r.outputs[3]) == V{2, 2});
    }
    SECTION("single-page document") {
        auto r = splitDocument(src("single.pdf"), tmp.path());
        CHECK(r.outputs.size() == 1);
    }
    SECTION("encrypted input keeps protection") {
        auto r = splitDocument(src("encrypted_user.pdf", "user"), tmp.path());
        REQUIRE(r.outputs.size() == 5);
        CHECK(markers(r.outputs[4], "user") == V{5});
    }
    SECTION("damaged input") {
        SplitOptions o;
        o.mode = SplitMode::EveryNPages;
        o.everyN = 5;
        auto r = splitDocument(src("broken_xref.pdf"), tmp.path(), o);
        CHECK(r.outputs.size() == 2);
        CHECK(markers(r.outputs[1]) == V{6, 7, 8, 9, 10});
    }
    for (const auto& f : tmp.files()) {
        CHECK(qpdfCheckPasses(tmp.path() / f,
                              openDocument(tmp.path() / f, "user").encrypted ? "user" : ""));
    }
}

TEST_CASE("splitDocument: refuses to overwrite and cleans up on failure") {
    TempDir tmp;
    {
        std::ofstream(tmp / "three-2.pdf") << "keep me";
    }
    CHECK(errorOf([&] { splitDocument(src("three.pdf"), tmp.path()); }) == ErrorKind::Io);
    CHECK(tmp.files().size() == 1);
    CHECK(readBytes(tmp / "three-2.pdf") == "keep me");

    SplitOptions o;
    o.mode = SplitMode::Ranges;
    o.ranges = {"1", "99"};
    CHECK(errorOf([&] { splitDocument(src("three.pdf"), tmp.path(), o); }) ==
          ErrorKind::InvalidArgument);
    o.ranges = {"1", "2"};
    o.nameTemplate = "same";
    CHECK(errorOf([&] { splitDocument(src("three.pdf"), tmp.path(), o); }) ==
          ErrorKind::InvalidArgument);
    o.nameTemplate = "a/{n}";
    CHECK(errorOf([&] { splitDocument(src("three.pdf"), tmp.path(), o); }) ==
          ErrorKind::InvalidArgument);

    CancelToken cancel;
    JobContext ctx{&cancel, [&cancel](int done, int total) {
                       if (done * 2 >= total) {
                           cancel.cancel();
                       }
                   }};
    CHECK(errorOf([&] { splitDocument(src("ten.pdf"), tmp.path(), {}, ctx); }) ==
          ErrorKind::Cancelled);
    CHECK(tmp.files().size() == 1); // partial parts removed

    SplitOptions overwrite;
    overwrite.overwrite = true;
    splitDocument(src("three.pdf"), tmp.path(), overwrite);
    CHECK(markers(tmp / "three-2.pdf") == V{2});
}

TEST_CASE("planSplit previews names without writing") {
    TempDir tmp;
    SplitOptions o;
    o.mode = SplitMode::EveryNPages;
    o.everyN = 3;
    auto names = planSplit(src("ten.pdf"), tmp.path(), o);
    CHECK(names.size() == 4);
    CHECK(tmp.files().empty());
}

// ---------------------------------------------------------------- encrypt / decrypt

TEST_CASE("encryptDocument / decryptDocument round trip") {
    TempDir tmp;
    const auto enc = tmp / "enc.pdf";
    const auto dec = tmp / "dec.pdf";
    Encryption e{"open-me", "admin", {}};
    e.permissions.print = false;
    encryptDocument(src("three.pdf"), e, enc);
    CHECK(errorOf([&] { openDocument(enc); }) == ErrorKind::PasswordRequired);
    CHECK(markers(enc, "open-me") == V{1, 2, 3});
    CHECK(qpdfCheckPasses(enc, "open-me"));

    int r = 0;
    int p = 0;
    REQUIRE(openDocument(enc, "admin").pdf->isEncrypted(r, p));
    CHECK(r == 6); // AES-256

    decryptDocument({0, enc, "open-me"}, dec);
    CHECK_FALSE(openDocument(dec).encrypted);
    CHECK(markers(dec) == V{1, 2, 3});
    CHECK(qpdfCheckPasses(dec));
}

TEST_CASE("encryptDocument: edge cases") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    SECTION("only a user password: it doubles as owner password") {
        encryptDocument(src("single.pdf"), {"pw", "", {}}, out);
        CHECK(markers(out, "pw") == V{1});
    }
    SECTION("only an owner password: opens without one, restrictions apply") {
        encryptDocument(src("single.pdf"), {"", "owner", {}}, out);
        CHECK(openDocument(out).encrypted);
    }
    SECTION("re-encrypting an encrypted file replaces the password") {
        encryptDocument(src("encrypted_rc4.pdf", "user"), {"new", "new-owner", {}}, out);
        CHECK(errorOf([&] { openDocument(out, "user"); }) == ErrorKind::WrongPassword);
        CHECK(markers(out, "new") == V{1, 2, 3, 4});
    }
    SECTION("damaged input") {
        encryptDocument(src("broken_xref.pdf"), {"x", "y", {}}, out);
        CHECK(markers(out, "x").size() == 10);
    }
    CHECK(errorOf([&] { encryptDocument(src("single.pdf"), {"", "", {}}, tmp / "x.pdf"); }) ==
          ErrorKind::InvalidArgument);
}

TEST_CASE("decryptDocument: needs the password, works in place") {
    TempDir tmp;
    CHECK(errorOf([&] { decryptDocument(src("encrypted_user.pdf"), tmp / "x.pdf"); }) ==
          ErrorKind::PasswordRequired);
    const auto path = testutil::copyFixture("encrypted_user.pdf", tmp);
    decryptDocument({0, path, "owner"}, path);
    CHECK_FALSE(openDocument(path).encrypted);
    CHECK(markers(path) == V{1, 2, 3, 4, 5});
    CHECK(tmp.files().size() == 1);
}

// ---------------------------------------------------------------- metadata

TEST_CASE("writeMetadata / readDocumentInfo") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    Metadata m;
    m.title = "Quarterly Report — Ünïcødé 报告";
    m.author = "Arsalan";
    m.keywords = "pdf, test";

    SECTION("plain") {
        writeMetadata(src("ten.pdf"), m, out);
        auto info = readDocumentInfo({0, out, {}});
        CHECK(info.metadata.title == m.title);
        CHECK(info.metadata.author == "Arsalan");
        CHECK(info.metadata.subject.empty());
        CHECK(info.metadata.modDate.starts_with("D:"));
        CHECK(info.pageCount == 10);
        CHECK(markers(out).size() == 10);
    }
    SECTION("clearing a field removes it") {
        writeMetadata(src("ten.pdf"), m, out);
        Metadata cleared = m;
        cleared.author.clear();
        writeMetadata({0, out, {}}, cleared, out);
        CHECK(readDocumentInfo({0, out, {}}).metadata.author.empty());
    }
    SECTION("encrypted input stays encrypted") {
        writeMetadata(src("encrypted_user.pdf", "user"), m, out);
        auto info = readDocumentInfo({0, out, "user"});
        CHECK(info.encrypted);
        CHECK(info.metadata.title == m.title);
    }
    SECTION("damaged input") {
        writeMetadata(src("broken_xref.pdf"), m, out);
        CHECK(readDocumentInfo({0, out, {}}).metadata.title == m.title);
    }
    CHECK(qpdfCheckPasses(out, openDocument(out, "user").encrypted ? "user" : ""));
    CHECK(errorOf([&] { readDocumentInfo(src("not_a_pdf.pdf")); }) == ErrorKind::NotAPdf);
}

// ---------------------------------------------------------------- compress

TEST_CASE("compressDocument: lossless keeps every image and page") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    auto report = compressDocument(src("images.pdf"), out);
    CHECK(report.imagesReplaced == 0);
    CHECK(markers(out) == V{1, 2});
    CHECK(countFilter(imageFilters(out), "/DCTDecode") == 0);
    CHECK(report.bytesAfter > 0);
    CHECK(qpdfCheckPasses(out));
}

TEST_CASE("compressDocument: lossy pass replaces only safe images, only when smaller") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    DctRecoder recoder;
    CompressOptions opts;
    opts.recoder = &recoder;
    auto report = compressDocument(src("images.pdf"), out, opts);

    // Eligible: rgb (shared, counted once), gray, masked rgb. Skipped: cmyk, /Decode, tiny,
    // and the soft mask itself is never reached as a page image.
    CHECK(report.imagesReplaced == 3);
    CHECK(recoder.calls == 3);
    CHECK(report.imagesSkipped >= 3);
    CHECK(report.bytesAfter < report.bytesBefore);
    auto filters = imageFilters(out);
    CHECK(countFilter(filters, "/DCTDecode") == 3);
    CHECK(markers(out) == V{1, 2});
    CHECK(qpdfCheckPasses(out));
}

TEST_CASE("compressDocument: images under a soft mask keep their size") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    DctRecoder halving(true);
    CompressOptions opts;
    opts.recoder = &halving;
    compressDocument(src("images.pdf"), out, opts);
    auto doc = openDocument(out);
    for (auto& page : QPDFPageDocumentHelper(*doc.pdf).getAllPages()) {
        page.forEachImage(false, [](QPDFObjectHandle& img, QPDFObjectHandle&, const std::string&) {
            auto d = img.getDict();
            if (d.hasKey("/SMask")) {
                CHECK(d.getKey("/Width").getIntValue() == 128);
            }
        });
    }
    CHECK(qpdfCheckPasses(out));
}

TEST_CASE("compressDocument: a recoder that cannot do better changes nothing") {
    struct Bloated : ImageRecoder {
        std::optional<EncodedJpeg> recode(const RawImage& img) override {
            return EncodedJpeg{img.width, img.height, std::string(10'000'000, 'x')};
        }
    } bloated;
    TempDir tmp;
    CompressOptions opts;
    opts.recoder = &bloated;
    auto report = compressDocument(src("images.pdf"), tmp / "out.pdf", opts);
    CHECK(report.imagesReplaced == 0);
    CHECK(countFilter(imageFilters(tmp / "out.pdf"), "/DCTDecode") == 0);
}

TEST_CASE("compressDocument: encrypted and damaged input, range-free op") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    DctRecoder recoder;
    CompressOptions opts;
    opts.recoder = &recoder;
    SECTION("encrypted") {
        auto report = compressDocument(src("images_encrypted.pdf", "user"), out, opts);
        CHECK(report.imagesReplaced == 3);
        CHECK(openDocument(out, "user").encrypted);
        CHECK(qpdfCheckPasses(out, "user"));
    }
    SECTION("damaged") {
        compressDocument(src("broken_xref.pdf"), out, opts);
        CHECK(markers(out).size() == 10);
        CHECK(qpdfCheckPasses(out));
    }
    SECTION("in place") {
        const auto path = testutil::copyFixture("images.pdf", tmp);
        compressDocument({0, path, {}}, path, opts);
        CHECK(markers(path) == V{1, 2});
        CHECK(tmp.files().size() == 1);
    }
}

// ---------------------------------------------------------------- images to PDF

TEST_CASE("parseJpegHeader") {
    auto photo = parseJpegHeader(readBytes(fixture("photo.jpg")));
    REQUIRE(photo);
    CHECK(photo->width == 320);
    CHECK(photo->height == 240);
    CHECK(photo->components == 3);
    auto gray = parseJpegHeader(readBytes(fixture("gray.jpg")));
    REQUIRE(gray);
    CHECK(gray->components == 1);
    CHECK(parseJpegHeader(readBytes(fixture("cmyk.jpg")))->components == 4);
    CHECK_FALSE(parseJpegHeader("not a jpeg"));
    CHECK_FALSE(parseJpegHeader(readBytes(fixture("photo.jpg")).substr(0, 20)));
    CHECK_FALSE(parseJpegHeader(""));
}

TEST_CASE("imagesToPdf") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    ImageInput photo{ImageInput::Kind::Jpeg, readBytes(fixture("photo.jpg"))};
    ImageInput gray{ImageInput::Kind::Jpeg, readBytes(fixture("gray.jpg"))};
    ImageInput pixels{ImageInput::Kind::Pixels, std::string(100 * 50 * 3, '\x80'), 100, 50, 3};

    SECTION("fit to image: JPEGs embedded byte-for-byte") {
        imagesToPdf({photo, gray, pixels}, out);
        auto doc = openDocument(out);
        auto pages = QPDFPageDocumentHelper(*doc.pdf).getAllPages();
        REQUIRE(pages.size() == 3);
        auto box = pages[0].getMediaBox().getArrayAsRectangle();
        CHECK(box.urx == Catch::Approx(240.0)); // 320 px at 96 dpi
        CHECK(box.ury == Catch::Approx(180.0));
        auto images = pages[0].getImages();
        REQUIRE(images.size() == 1);
        auto img = images.begin()->second;
        CHECK(img.getDict().getKey("/Filter").getName() == "/DCTDecode");
        auto raw = img.getRawStreamData();
        CHECK(std::string(reinterpret_cast<const char*>(raw->getBuffer()), raw->getSize()) ==
              photo.data);
    }
    SECTION("A4 with margins, auto orientation") {
        ImagesToPdfOptions o;
        o.pageSize = PageSize::A4;
        o.marginPoints = 36;
        imagesToPdf({photo, gray}, out, o);
        auto doc = openDocument(out);
        auto pages = QPDFPageDocumentHelper(*doc.pdf).getAllPages();
        auto landscape = pages[0].getMediaBox().getArrayAsRectangle();
        auto portrait = pages[1].getMediaBox().getArrayAsRectangle();
        CHECK(landscape.urx > landscape.ury);
        CHECK(portrait.urx < portrait.ury);
    }
    SECTION("encrypted output") {
        ImagesToPdfOptions o;
        o.write.encryptionMode = EncryptionMode::Set;
        o.write.encryption = Encryption{"u", "o", {}};
        imagesToPdf({photo}, out, o);
        CHECK(openDocument(out, "u").encrypted);
        CHECK(qpdfCheckPasses(out, "u"));
        return;
    }
    CHECK(qpdfCheckPasses(out));
}

TEST_CASE("imagesToPdf: rejects bad input without writing") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    CHECK(errorOf([&] { imagesToPdf({}, out); }) == ErrorKind::InvalidArgument);
    CHECK(errorOf([&] {
              imagesToPdf({{ImageInput::Kind::Jpeg, readBytes(fixture("cmyk.jpg"))}}, out);
          }) == ErrorKind::InvalidArgument);
    CHECK(errorOf([&] { imagesToPdf({{ImageInput::Kind::Jpeg, "garbage"}}, out); }) ==
          ErrorKind::InvalidArgument);
    CHECK(errorOf([&] {
              imagesToPdf({{ImageInput::Kind::Pixels, std::string(10, 'x'), 100, 100, 3}}, out);
          }) == ErrorKind::InvalidArgument);
    CHECK(tmp.files().empty());
}
