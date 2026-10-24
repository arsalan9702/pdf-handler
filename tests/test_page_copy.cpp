#include "TestUtil.hpp"
#include "core/PageCopy.hpp"

#include <catch2/catch_test_macros.hpp>
#include <qpdf/QPDFAcroFormDocumentHelper.hh>

using namespace pdfcore;
using testutil::fixture;
using testutil::markers;
using testutil::qpdfCheckPasses;
using testutil::TempDir;
using V = std::vector<int>;

namespace {

Project singleSource(const std::filesystem::path& path, const V& pages, const std::string& pw = {},
                     int rotation = 0) {
    Project p;
    p.sources.push_back({1, path, pw});
    for (int i : pages) {
        p.pages.push_back({1, i, rotation});
    }
    return p;
}

ErrorKind writeError(const Project& p, const std::filesystem::path& out,
                     const WriteOptions& opts = {}, const JobContext& ctx = {}) {
    try {
        writePages(p, out, opts, ctx);
    } catch (const PdfError& e) {
        return e.kind();
    }
    FAIL("expected writePages to throw");
    return ErrorKind::InvalidArgument;
}

} // namespace

TEST_CASE("writePages single source: selection, order and duplicates") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";

    SECTION("first page") {
        writePages(singleSource(fixture("ten.pdf"), {0}), out);
        CHECK(markers(out) == V{1});
    }
    SECTION("last page") {
        writePages(singleSource(fixture("ten.pdf"), {9}), out);
        CHECK(markers(out) == V{10});
    }
    SECTION("single-page document") {
        writePages(singleSource(fixture("single.pdf"), {0}), out);
        CHECK(markers(out) == V{1});
    }
    SECTION("reordered") {
        writePages(singleSource(fixture("ten.pdf"), {9, 0, 4, 5}), out);
        CHECK(markers(out) == V{10, 1, 5, 6});
    }
    SECTION("duplicates") {
        writePages(singleSource(fixture("ten.pdf"), {2, 2, 0, 2}), out);
        CHECK(markers(out) == V{3, 3, 1, 3});
    }
    CHECK(qpdfCheckPasses(out));
    CHECK(tmp.files().size() == 1); // no temp files left behind
}

TEST_CASE("writePages: out-of-range pages are rejected and nothing is written") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    CHECK(writeError(singleSource(fixture("ten.pdf"), {10}), out) == ErrorKind::InvalidArgument);
    CHECK(writeError(singleSource(fixture("ten.pdf"), {-1}), out) == ErrorKind::InvalidArgument);
    CHECK(writeError(singleSource(fixture("ten.pdf"), {}), out) == ErrorKind::InvalidArgument);
    CHECK(writeError(singleSource(fixture("ten.pdf"), {0}, {}, 45), out) ==
          ErrorKind::InvalidArgument);
    CHECK(tmp.files().empty());
}

TEST_CASE("writePages: rotation is relative to the source page, inheritance included") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    // mixed.pdf effective rotations: 180 (inherited), 180 (inherited), 90, 270
    CHECK(testutil::rotations(fixture("mixed.pdf")) == V{180, 180, 90, 270});

    SECTION("single source") {
        writePages(singleSource(fixture("mixed.pdf"), {0, 1, 2, 3}, {}, 90), out);
    }
    SECTION("multi source") {
        auto p = singleSource(fixture("mixed.pdf"), {0, 1, 2, 3}, {}, 90);
        p.sources.push_back({2, fixture("single.pdf"), {}});
        p.pages.push_back({2, 0, -90});
        writePages(p, out);
        CHECK(testutil::rotations(out).back() == 270);
        CHECK(markers(out).size() == 5);
    }
    auto r = testutil::rotations(out);
    CHECK(V(r.begin(), r.begin() + 4) == V{270, 270, 180, 0});
    CHECK(qpdfCheckPasses(out));
}

TEST_CASE("writePages multi source: interleaved order and duplicates") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    Project p;
    p.sources = {{1, fixture("ten.pdf"), {}}, {2, fixture("three.pdf"), {}}};
    p.pages = {{2, 0, 0}, {1, 9, 0}, {2, 2, 0}, {1, 0, 0}, {2, 0, 0}};
    writePages(p, out);
    CHECK(markers(out) == V{1, 10, 3, 1, 1});
    CHECK(qpdfCheckPasses(out));
}

TEST_CASE("writePages keeps form fields and links") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";

    SECTION("single source") {
        writePages(singleSource(fixture("interactive.pdf"), {0, 2}), out);
    }
    SECTION("merged with another document") {
        Project p;
        p.sources = {{1, fixture("three.pdf"), {}}, {2, fixture("interactive.pdf"), {}}};
        p.pages = {{1, 0, 0}, {2, 0, 0}};
        writePages(p, out);
    }
    auto doc = openDocument(out);
    QPDFAcroFormDocumentHelper forms(*doc.pdf);
    CHECK(forms.hasAcroForm());
    CHECK(forms.getFormFields().size() == 1);
    bool hasLink = false;
    for (auto& page : QPDFPageDocumentHelper(*doc.pdf).getAllPages()) {
        for (auto& annot : page.getAnnotations("/Link")) {
            hasLink = hasLink || annot.getObjectHandle().isDictionary();
        }
    }
    CHECK(hasLink);
    CHECK(qpdfCheckPasses(out));
}

TEST_CASE("writePages: encrypted input") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";

    CHECK(writeError(singleSource(fixture("encrypted_user.pdf"), {0}), out) ==
          ErrorKind::PasswordRequired);
    CHECK(writeError(singleSource(fixture("encrypted_user.pdf"), {0}, "bad"), out) ==
          ErrorKind::WrongPassword);

    SECTION("preserve keeps the original protection") {
        writePages(singleSource(fixture("encrypted_user.pdf"), {4, 0}, "user"), out);
        CHECK(openDocument(out, "user").encrypted);
        CHECK(markers(out, "user") == V{5, 1});
        CHECK(qpdfCheckPasses(out, "user"));
    }
    SECTION("remove decrypts") {
        WriteOptions opts;
        opts.encryptionMode = EncryptionMode::Remove;
        writePages(singleSource(fixture("encrypted_user.pdf"), {1}, "owner"), out, opts);
        CHECK_FALSE(openDocument(out).encrypted);
        CHECK(markers(out) == V{2});
    }
    SECTION("set applies new AES-256 passwords") {
        WriteOptions opts;
        opts.encryptionMode = EncryptionMode::Set;
        opts.encryption = Encryption{"new-user", "new-owner", {}};
        writePages(singleSource(fixture("encrypted_rc4.pdf"), {0, 1}, "user"), out, opts);
        CHECK(markers(out, "new-user") == V{1, 2});
        CHECK(markers(out, "new-owner") == V{1, 2});
    }
    SECTION("merging encrypted sources") {
        Project p;
        p.sources = {{1, fixture("encrypted_user.pdf"), "user"},
                     {2, fixture("encrypted_owner_only.pdf"), {}}};
        p.pages = {{1, 0, 0}, {2, 1, 0}};
        writePages(p, out);
        CHECK_FALSE(openDocument(out).encrypted);
        CHECK(markers(out) == V{1, 2});
    }
}

TEST_CASE("writePages: damaged input produces a clean, validated output") {
    TempDir tmp;
    const auto out = tmp / "out.pdf";
    auto result = writePages(singleSource(fixture("broken_xref.pdf"), {0, 9}), out);
    CHECK_FALSE(result.warnings.empty());
    CHECK(markers(out) == V{1, 10});
    CHECK(qpdfCheckPasses(out));
    CHECK(openDocument(out).warnings.empty());
}

TEST_CASE("writePages: never damages the target when something goes wrong") {
    TempDir tmp;
    const auto out = tmp / "existing.pdf";
    std::filesystem::copy_file(fixture("three.pdf"), out);
    const auto original = testutil::readFile(out);

    SECTION("cancelled before starting") {
        CancelToken cancel;
        cancel.cancel();
        JobContext ctx{&cancel, {}};
        CHECK(writeError(singleSource(fixture("ten.pdf"), {0, 1}), out, {}, ctx) ==
              ErrorKind::Cancelled);
    }
    SECTION("cancelled during the write phase") {
        CancelToken cancel;
        JobContext ctx{&cancel, [&cancel](int done, int total) {
                           if (done > total / 2) {
                               cancel.cancel();
                           }
                       }};
        auto p = singleSource(fixture("ten.pdf"), {0, 1, 2});
        p.sources.push_back({2, fixture("three.pdf"), {}});
        p.pages.push_back({2, 0, 0});
        CHECK(writeError(p, out, {}, ctx) == ErrorKind::Cancelled);
    }
    SECTION("source cannot be opened") {
        CHECK(writeError(singleSource(fixture("not_a_pdf.pdf"), {0}), out) == ErrorKind::NotAPdf);
    }
    CHECK(testutil::readFile(out) == original);
    CHECK(tmp.files().size() == 1);
}

TEST_CASE("writePages: output may overwrite one of its own inputs") {
    TempDir tmp;
    const auto path = testutil::copyFixture("ten.pdf", tmp);
    writePages(singleSource(path, {9, 8, 7}), path);
    CHECK(markers(path) == V{10, 9, 8});

    const auto other = testutil::copyFixture("three.pdf", tmp);
    Project p;
    p.sources = {{1, path, {}}, {2, other, {}}};
    p.pages = {{2, 0, 0}, {1, 0, 0}};
    writePages(p, path);
    CHECK(markers(path) == V{1, 10});
    CHECK(qpdfCheckPasses(path));
    CHECK(tmp.files().size() == 2);
}

TEST_CASE("writePages: progress is monotonic and finishes at the total") {
    TempDir tmp;
    std::vector<std::pair<int, int>> calls;
    JobContext ctx{nullptr, [&calls](int d, int t) { calls.emplace_back(d, t); }};
    writePages(singleSource(fixture("ten.pdf"), {0, 1, 2, 3}), tmp / "out.pdf", {}, ctx);
    REQUIRE_FALSE(calls.empty());
    for (std::size_t i = 1; i < calls.size(); ++i) {
        CHECK(calls[i].first >= calls[i - 1].first);
    }
    CHECK(calls.back() == std::pair{8, 8});
}

TEST_CASE("writePages: output folder must exist") {
    TempDir tmp;
    CHECK(writeError(singleSource(fixture("ten.pdf"), {0}), tmp / "missing" / "out.pdf") ==
          ErrorKind::Io);
}

TEST_CASE("writePages: non-ASCII paths") {
    TempDir tmp;
    const auto out = tmp / "résumé 出力.pdf";
    writePages(singleSource(fixture("ünïcødé 文件.pdf"), {2, 0}), out);
    CHECK(markers(out) == V{3, 1});
}
