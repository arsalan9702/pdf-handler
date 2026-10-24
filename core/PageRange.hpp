#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace pdfcore {

struct RangeError {
    std::string message;
    std::size_t position = 0; // offset into the input where the problem starts
};

// Parses a 1-based page range string into 0-based page indices, in the order written.
//
// Grammar (whitespace ignored):  item ("," item)*
//   item := N | N "-" M | N "-" | "-" M | "z"
// "z" means the last page. "8-" means page 8 to the end, "-3" means pages 1-3.
// "5-3" is a reverse range (5,4,3). Duplicates are kept; callers that need unique pages
// dedupe themselves.
std::expected<std::vector<int>, RangeError> parsePageRange(std::string_view text, int pageCount);

// Formats 0-based indices as a compact 1-based range string ("1-3,5,8-10").
std::string formatPageRange(const std::vector<int>& indices);

} // namespace pdfcore
