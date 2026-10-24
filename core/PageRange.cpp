#include "core/PageRange.hpp"

#include <cctype>
#include <optional>

namespace pdfcore {

namespace {

class Parser {
  public:
    Parser(std::string_view text, int pageCount) : m_text(text), m_pageCount(pageCount) {}

    std::expected<std::vector<int>, RangeError> run() {
        if (m_pageCount <= 0) {
            return fail("Document has no pages", 0);
        }
        skipSpace();
        if (atEnd()) {
            return fail("Enter at least one page", 0);
        }
        std::vector<int> result;
        while (true) {
            if (auto err = parseItem(result)) {
                return std::unexpected(*err);
            }
            skipSpace();
            if (atEnd()) {
                break;
            }
            if (peek() != ',') {
                return fail("Expected ',' between page ranges", m_pos);
            }
            ++m_pos;
            skipSpace();
            if (atEnd()) {
                return fail("Trailing ','", m_pos);
            }
        }
        return result;
    }

  private:
    std::optional<RangeError> parseItem(std::vector<int>& out) {
        const std::size_t start = m_pos;
        std::optional<int> first;
        if (peek() != '-') {
            auto n = parseNumber();
            if (!n) {
                return RangeError{.message = "Expected a page number", .position = start};
            }
            first = n;
        }
        skipSpace();
        if (!atEnd() && peek() == '-') {
            ++m_pos;
            skipSpace();
            std::optional<int> last;
            if (!atEnd() && peek() != ',') {
                const std::size_t lastPos = m_pos;
                last = parseNumber();
                if (!last) {
                    return RangeError{.message = "Expected a page number after '-'",
                                      .position = lastPos};
                }
            }
            if (!first && !last) {
                return RangeError{.message = "A range needs at least one page number",
                                  .position = start};
            }
            const int from = first.value_or(1);
            const int to = last.value_or(m_pageCount);
            if (auto err = checkBounds(from, start)) {
                return err;
            }
            if (auto err = checkBounds(to, start)) {
                return err;
            }
            const int step = from <= to ? 1 : -1;
            for (int p = from;; p += step) {
                out.push_back(p - 1);
                if (p == to) {
                    break;
                }
            }
            return std::nullopt;
        }
        if (!first) {
            return RangeError{.message = "Expected a page number", .position = start};
        }
        if (auto err = checkBounds(*first, start)) {
            return err;
        }
        out.push_back(*first - 1);
        return std::nullopt;
    }

    // Returns a 1-based page number. "z" is the last page.
    std::optional<int> parseNumber() {
        if (atEnd()) {
            return std::nullopt;
        }
        if (peek() == 'z' || peek() == 'Z') {
            ++m_pos;
            return m_pageCount;
        }
        if (std::isdigit(static_cast<unsigned char>(peek())) == 0) {
            return std::nullopt;
        }
        long long value = 0;
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            value = value * 10 + (peek() - '0');
            if (value > 1'000'000'000) {
                value = 1'000'000'001; // clamp; reported as out of range
            }
            ++m_pos;
        }
        return static_cast<int>(value);
    }

    [[nodiscard]] std::optional<RangeError> checkBounds(int page, std::size_t pos) const {
        if (page < 1 || page > m_pageCount) {
            return RangeError{.message = "Page " + std::to_string(page) + " is out of range (1-" +
                                         std::to_string(m_pageCount) + ")",
                              .position = pos};
        }
        return std::nullopt;
    }

    [[nodiscard]] std::unexpected<RangeError> fail(std::string message, std::size_t pos) const {
        return std::unexpected(RangeError{.message = std::move(message), .position = pos});
    }

    void skipSpace() {
        while (!atEnd() && std::isspace(static_cast<unsigned char>(peek())) != 0) {
            ++m_pos;
        }
    }
    [[nodiscard]] bool atEnd() const { return m_pos >= m_text.size(); }
    [[nodiscard]] char peek() const { return m_text[m_pos]; }

    std::string_view m_text;
    int m_pageCount;
    std::size_t m_pos = 0;
};

} // namespace

std::expected<std::vector<int>, RangeError> parsePageRange(std::string_view text, int pageCount) {
    return Parser(text, pageCount).run();
}

std::string formatPageRange(const std::vector<int>& indices) {
    std::string out;
    std::size_t i = 0;
    while (i < indices.size()) {
        std::size_t j = i;
        while (j + 1 < indices.size() && indices[j + 1] == indices[j] + 1) {
            ++j;
        }
        if (!out.empty()) {
            out += ',';
        }
        out += std::to_string(indices[i] + 1);
        if (j > i) {
            out += '-';
            out += std::to_string(indices[j] + 1);
        }
        i = j + 1;
    }
    return out;
}

} // namespace pdfcore
