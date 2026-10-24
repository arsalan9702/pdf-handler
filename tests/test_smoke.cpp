#include "core/Version.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("core links against QPDF 12+") {
    const auto version = pdfcore::qpdfVersion();
    REQUIRE_FALSE(version.empty());
    REQUIRE(std::stoi(version) >= 12);
}
