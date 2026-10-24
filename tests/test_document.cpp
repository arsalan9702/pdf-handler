#include "TestUtil.hpp"
#include "core/Document.hpp"
#include "core/Errors.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace pdfcore;
using testutil::fixture;

namespace {

ErrorKind openError(const std::filesystem::path& path, const std::string& password = {}) {
    try {
        openDocument(path, password);
    } catch (const PdfError& e) {
        return e.kind();
    }
    FAIL("expected openDocument to throw");
    return ErrorKind::InvalidArgument;
}

} // namespace

TEST_CASE("openDocument: plain documents") {
    auto doc = openDocument(fixture("ten.pdf"));
    CHECK(doc.pageCount == 10);
    CHECK_FALSE(doc.encrypted);
    CHECK(doc.warnings.empty());
    CHECK(testutil::markers(fixture("ten.pdf")) == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
    CHECK(openDocument(fixture("single.pdf")).pageCount == 1);
    CHECK(openDocument(fixture("objstreams.pdf")).pageCount == 6);
    CHECK(openDocument(fixture("linearized.pdf")).pageCount == 6);
}

TEST_CASE("openDocument: encrypted documents") {
    CHECK(openError(fixture("encrypted_user.pdf")) == ErrorKind::PasswordRequired);
    CHECK(openError(fixture("encrypted_user.pdf"), "nope") == ErrorKind::WrongPassword);

    auto withUser = openDocument(fixture("encrypted_user.pdf"), "user");
    CHECK(withUser.encrypted);
    CHECK(withUser.pageCount == 5);
    CHECK(openDocument(fixture("encrypted_user.pdf"), "owner").pageCount == 5);

    auto ownerOnly = openDocument(fixture("encrypted_owner_only.pdf"));
    CHECK(ownerOnly.encrypted);
    CHECK(ownerOnly.pageCount == 5);

    CHECK(openDocument(fixture("encrypted_rc4.pdf"), "user").pageCount == 4);
}

TEST_CASE("openDocument: damaged files are repaired and the repair is reported") {
    auto doc = openDocument(fixture("broken_xref.pdf"));
    CHECK(doc.pageCount == 10);
    CHECK_FALSE(doc.warnings.empty());
}

TEST_CASE("openDocument: truncated files either recover with warnings or fail cleanly") {
    try {
        auto doc = openDocument(fixture("truncated.pdf"));
        CHECK(doc.pageCount > 0);
        CHECK_FALSE(doc.warnings.empty());
    } catch (const PdfError& e) {
        CHECK(e.kind() == ErrorKind::Damaged);
    }
}

TEST_CASE("openDocument: non-PDF and missing files") {
    CHECK(openError(fixture("not_a_pdf.pdf")) == ErrorKind::NotAPdf);
    CHECK(openError(fixture("does_not_exist.pdf")) == ErrorKind::Io);
}

TEST_CASE("openDocument: non-ASCII file names") {
    CHECK(openDocument(fixture("ünïcødé 文件.pdf")).pageCount == 3);
}
