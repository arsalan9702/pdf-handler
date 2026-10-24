#pragma once

#include "core/Document.hpp"
#include "core/SafeWriter.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <random>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace testutil {

namespace fs = std::filesystem;

inline fs::path fixture(const std::string& name) {
    return fs::path(PDFH_FIXTURE_DIR) / fs::path(reinterpret_cast<const char8_t*>(name.c_str()));
}

// A fresh directory per test, removed afterwards.
class TempDir {
  public:
    TempDir() {
        std::random_device rd;
        m_path = fs::temp_directory_path() / ("pdfh-test-" + std::to_string(rd()));
        fs::create_directories(m_path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(m_path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] fs::path operator/(const std::string& name) const {
        return m_path / fs::path(reinterpret_cast<const char8_t*>(name.c_str()));
    }
    [[nodiscard]] const fs::path& path() const { return m_path; }

    [[nodiscard]] std::vector<fs::path> files() const {
        std::vector<fs::path> out;
        for (const auto& e : fs::directory_iterator(m_path)) {
            out.push_back(e.path().filename());
        }
        return out;
    }

  private:
    fs::path m_path;
};

inline fs::path copyFixture(const std::string& name, const TempDir& dir) {
    auto dest = dir / name;
    fs::copy_file(fixture(name), dest, fs::copy_options::overwrite_existing);
    return dest;
}

// The /PdfhMarker of each page, in order.
inline std::vector<int> markers(const fs::path& path, const std::string& password = {}) {
    auto doc = pdfcore::openDocument(path, password);
    std::vector<int> out;
    for (auto& page : QPDFPageDocumentHelper(*doc.pdf).getAllPages()) {
        auto marker = page.getObjectHandle().getKey("/PdfhMarker");
        out.push_back(marker.isInteger() ? marker.getIntValueAsInt() : -1);
    }
    return out;
}

// Effective /Rotate of each page (inheritance resolved), normalized to 0..270.
inline std::vector<int> rotations(const fs::path& path) {
    auto doc = pdfcore::openDocument(path);
    std::vector<int> out;
    for (auto& page : QPDFPageDocumentHelper(*doc.pdf).getAllPages()) {
        auto r = page.getAttribute("/Rotate", false);
        const int v = r.isInteger() ? r.getIntValueAsInt() : 0;
        out.push_back(((v % 360) + 360) % 360);
    }
    return out;
}

inline std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

// `qpdf --check` via the CLI when available, otherwise the in-library equivalent.
inline bool qpdfCheckPasses(const fs::path& path, const std::string& password = {}) {
    const std::string exe = PDFH_QPDF_EXECUTABLE;
    if (exe.empty()) {
        try {
            pdfcore::validatePdf(path, password, std::nullopt);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    std::string cmd = "\"" + exe + "\" --check";
    if (!password.empty()) {
        cmd += " --password=" + password;
    }
    cmd += " \"" + pdfcore::pathToUtf8(path) + "\"";
#ifdef _WIN32
    cmd = "\"" + cmd + " > NUL 2>&1\"";
#else
    cmd += " > /dev/null 2>&1";
#endif
    // Exit code 0 = clean, 3 = warnings only; both mean a structurally valid file.
    const int status = std::system(cmd.c_str());
#ifdef _WIN32
    const int code = status;
#else
    const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    return code == 0 || code == 3;
}

} // namespace testutil
