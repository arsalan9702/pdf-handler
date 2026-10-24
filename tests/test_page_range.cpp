#include "core/PageRange.hpp"

#include <catch2/catch_test_macros.hpp>

using pdfcore::formatPageRange;
using pdfcore::parsePageRange;
using V = std::vector<int>;

TEST_CASE("parsePageRange: single pages and simple ranges") {
    CHECK(parsePageRange("1", 10).value() == V{0});
    CHECK(parsePageRange("10", 10).value() == V{9});
    CHECK(parsePageRange("1-3", 10).value() == V{0, 1, 2});
    CHECK(parsePageRange("2,4,6", 10).value() == V{1, 3, 5});
    CHECK(parsePageRange(" 1 - 2 , 5 ", 10).value() == V{0, 1, 4});
}

TEST_CASE("parsePageRange: first, last and single-page documents") {
    CHECK(parsePageRange("1", 1).value() == V{0});
    CHECK(parsePageRange("z", 1).value() == V{0});
    CHECK(parsePageRange("1-z", 1).value() == V{0});
    CHECK(parsePageRange("z", 10).value() == V{9});
    CHECK(parsePageRange("9-z", 10).value() == V{8, 9});
}

TEST_CASE("parsePageRange: open-ended ranges") {
    CHECK(parsePageRange("8-", 10).value() == V{7, 8, 9});
    CHECK(parsePageRange("-3", 10).value() == V{0, 1, 2});
    CHECK(parsePageRange("10-", 10).value() == V{9});
    CHECK(parsePageRange("1-,3", 3).value() == V{0, 1, 2, 2});
}

TEST_CASE("parsePageRange: reverse ranges and duplicates are kept in written order") {
    CHECK(parsePageRange("5-3", 10).value() == V{4, 3, 2});
    CHECK(parsePageRange("z-1", 3).value() == V{2, 1, 0});
    CHECK(parsePageRange("1,1,2-3,2", 5).value() == V{0, 0, 1, 2, 1});
}

TEST_CASE("parsePageRange: out of range and malformed input are errors") {
    CHECK_FALSE(parsePageRange("0", 10).has_value());
    CHECK_FALSE(parsePageRange("11", 10).has_value());
    CHECK_FALSE(parsePageRange("3-11", 10).has_value());
    CHECK_FALSE(parsePageRange("99999999999999", 10).has_value());
    CHECK_FALSE(parsePageRange("", 10).has_value());
    CHECK_FALSE(parsePageRange("   ", 10).has_value());
    CHECK_FALSE(parsePageRange("-", 10).has_value());
    CHECK_FALSE(parsePageRange("1,", 10).has_value());
    CHECK_FALSE(parsePageRange(",1", 10).has_value());
    CHECK_FALSE(parsePageRange("1;2", 10).has_value());
    CHECK_FALSE(parsePageRange("a", 10).has_value());
    CHECK_FALSE(parsePageRange("1-2-3", 10).has_value());
    CHECK_FALSE(parsePageRange("1", 0).has_value());

    auto err = parsePageRange("1,2,x", 10).error();
    CHECK(err.position == 4);
    auto oob = parsePageRange("12", 10).error();
    CHECK(oob.message.find("out of range") != std::string::npos);
}

TEST_CASE("formatPageRange compacts consecutive runs and round-trips") {
    CHECK(formatPageRange({0, 1, 2, 4, 7, 8, 9}) == "1-3,5,8-10");
    CHECK(formatPageRange({4, 3, 2}) == "5,4,3");
    CHECK(formatPageRange({0, 0}) == "1,1");
    CHECK(formatPageRange({}).empty());
    const V pages{2, 0, 1, 1, 5, 6, 7};
    CHECK(parsePageRange(formatPageRange(pages), 10).value() == pages);
}
