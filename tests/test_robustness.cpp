// Mutation testing ("dumb fuzzing"): damaged variants of every fixture must either produce a
// verified output or fail with a PdfError -- never crash, hang, or leave a bad file behind.

#include "TestUtil.hpp"
#include "core/PageCopy.hpp"
#include "core/ops/Compress.hpp"
#include "core/ops/Split.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <qpdf/QPDFWriter.hh>
#include <random>

using namespace pdfcore;
using testutil::fixture;
using testutil::TempDir;

namespace {

std::string mutate(std::string data, std::mt19937& rng) {
    if (data.empty()) {
        return data;
    }
    auto pick = [&](std::size_t n) {
        return std::uniform_int_distribution<std::size_t>(0, n - 1)(rng);
    };
    const int edits = 1 + static_cast<int>(pick(8));
    for (int e = 0; e < edits && !data.empty(); ++e) {
        switch (pick(7)) {
        case 0: // flip a bit
            data[pick(data.size())] ^= static_cast<char>(1 << pick(8));
            break;
        case 1: // truncate
            data.resize(pick(data.size()));
            break;
        case 2: { // delete a chunk
            const auto at = pick(data.size());
            data.erase(at, pick(std::min<std::size_t>(512, data.size() - at) + 1));
            break;
        }
        case 3: { // duplicate a chunk elsewhere
            const auto from = pick(data.size());
            const auto len = pick(std::min<std::size_t>(256, data.size() - from) + 1);
            data.insert(pick(data.size()), data.substr(from, len));
            break;
        }
        case 4: { // corrupt a number (offsets, lengths, object ids)
            const auto at = data.find_first_of("0123456789", pick(data.size()));
            if (at != std::string::npos) {
                data[at] = static_cast<char>('0' + pick(10));
                if (pick(2) == 0) {
                    data.insert(at, std::to_string(pick(1'000'000'000)));
                }
            }
            break;
        }
        case 5: { // break a keyword
            static const char* words[] = {"obj",     "endobj",    "stream",  "endstream", "xref",
                                          "trailer", "startxref", "/Length", "/Pages",    "/Kids"};
            const std::string w = words[pick(10)];
            if (const auto at = data.find(w, pick(data.size())); at != std::string::npos) {
                data[at + pick(w.size())] = 'X';
            }
            break;
        }
        default: // random garbage
            data.insert(pick(data.size()), std::string(pick(64) + 1, static_cast<char>(pick(256))));
        }
    }
    return data;
}

struct Tally {
    int written = 0;
    int rejected = 0;
};

void exercise(const std::filesystem::path& damaged, const std::filesystem::path& out,
              Tally& tally) {
    try {
        auto doc = openDocument(damaged);
        Project p;
        p.sources.push_back({1, damaged, {}});
        // Last page first, then everything: exercises page lookup and both copy paths.
        p.pages.push_back({1, doc.pageCount - 1, 90});
        for (int i = 0; i < doc.pageCount; ++i) {
            p.pages.push_back({1, i, 0});
        }
        doc.pdf.reset();
        writePages(p, out);
        // Anything that was written must be a structurally valid PDF.
        REQUIRE(testutil::qpdfCheckPasses(out));
        ++tally.written;
    } catch (const PdfError&) {
        ++tally.rejected;
        REQUIRE_FALSE(std::filesystem::exists(out));
    }
}

} // namespace

TEST_CASE("Damaged inputs never crash or produce invalid output", "[robustness]") {
    TempDir tmp;
    const auto damaged = tmp / "damaged.pdf";
    const auto out = tmp / "out.pdf";
    Tally tally;
    std::mt19937 rng(20261024); // fixed seed: failures are reproducible
    const char* sources[] = {
        "ten.pdf",        "mixed.pdf",  "interactive.pdf",         "objstreams.pdf",
        "linearized.pdf", "images.pdf", "encrypted_owner_only.pdf"};
    const int perSource = 60;
    for (const char* name : sources) {
        const auto original = testutil::readFile(fixture(name));
        for (int i = 0; i < perSource; ++i) {
            {
                std::ofstream(damaged, std::ios::binary | std::ios::trunc) << mutate(original, rng);
            }
            INFO(name << " mutation " << i);
            exercise(damaged, out, tally);
            std::error_code ec;
            std::filesystem::remove(out, ec);
        }
    }
    // QPDF's recovery should rescue a good share of these.
    CHECK(tally.written > 0);
    CHECK(tally.written + tally.rejected == perSource * 7);
    CHECK(tmp.files().size() == 1); // only damaged.pdf; no temp files left behind
}

TEST_CASE("Large documents stay fast", "[large]") {
    TempDir tmp;
    const auto big = tmp / "big.pdf";
    constexpr int kPages = 2000;
    {
        // Built from ten.pdf's first page so every page carries a marker.
        auto doc = openDocument(fixture("ten.pdf"));
        QPDFPageDocumentHelper pages(*doc.pdf);
        auto first = pages.getAllPages().front();
        for (int i = 10; i < kPages; ++i) {
            auto copy = first.shallowCopyPage();
            copy.getObjectHandle().replaceKey("/PdfhMarker", QPDFObjectHandle::newInteger(i + 1));
            pages.addPage(copy, false);
        }
        const std::string name = pathToUtf8(big); // QPDFWriter keeps the pointer until write()
        QPDFWriter w(*doc.pdf, name.c_str());
        w.write();
    }
    using clock = std::chrono::steady_clock;

    auto t0 = clock::now();
    Project p;
    p.sources.push_back({1, big, {}});
    for (int i = kPages - 1; i >= 0; --i) {
        p.pages.push_back({1, i, i % 4 == 0 ? 90 : 0});
    }
    writePages(p, tmp / "reversed.pdf");
    const auto reverseMs = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0);
    auto m = testutil::markers(tmp / "reversed.pdf");
    REQUIRE(m.size() == kPages);
    CHECK(m.front() == kPages);
    CHECK(m.back() == 1);

    t0 = clock::now();
    SplitOptions o;
    o.mode = SplitMode::EveryNPages;
    o.everyN = 100;
    std::filesystem::create_directories(tmp / "parts");
    auto parts = splitDocument({1, big, {}}, tmp / "parts", o);
    const auto splitMs = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0);
    CHECK(parts.outputs.size() == kPages / 100);

    t0 = clock::now();
    compressDocument({1, big, {}}, tmp / "compressed.pdf");
    const auto compressMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - t0);

    WARN("2000 pages: reverse+rotate " << reverseMs.count() << " ms, split into 20 "
                                       << splitMs.count() << " ms, compress " << compressMs.count()
                                       << " ms");
    // Generous bounds; these catch accidental quadratic behaviour, not micro-regressions.
    CHECK(reverseMs.count() < 20000);
    CHECK(splitMs.count() < 30000);
    CHECK(compressMs.count() < 20000);
}
