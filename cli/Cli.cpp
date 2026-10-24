// pdfh: command-line front end for libpdfcore.

#include "cli/Cli.hpp"

#include "core/Document.hpp"
#include "core/Errors.hpp"
#include "core/Version.hpp"
#include "core/ops/Compress.hpp"
#include "core/ops/Encrypt.hpp"
#include "core/ops/Extract.hpp"
#include "core/ops/ImagesToPdf.hpp"
#include "core/ops/Merge.hpp"
#include "core/ops/Metadata.hpp"
#include "core/ops/Split.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
// clang-format off: shellapi.h needs windows.h first
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif

namespace fs = std::filesystem;
using namespace pdfcore;

namespace {

constexpr int kExitOk = 0;
constexpr int kExitError = 1;
constexpr int kExitUsage = 2;

constexpr const char* kUsage = R"(usage: pdfh <command> [options]

Page ranges are 1-based: "1-3,5,8-" (8 to end), "z" = last page, "5-3" = reversed.
Every command writes through a temporary file that is verified before it replaces
the output, so a failure never leaves a damaged file. -o may name the input file.

  info      FILE [--password PW]
  extract   FILE RANGE -o OUT [--password PW]
  delete    FILE RANGE -o OUT [--password PW]
  rotate    FILE DEGREES [RANGE] -o OUT [--password PW]
  merge     -o OUT FILE [--pages RANGE] [--password PW] FILE ... [--bookmarks]
            (--pages/--password apply to the file before them)
  split     FILE -d DIR [--every N | --ranges "1-3;4-"] [--name "{name}-{n}"]
            [--overwrite] [--password PW]
  encrypt   FILE -o OUT --user PW [--owner PW] [--no-print] [--no-copy]
            [--no-modify] [--password PW]
  decrypt   FILE -o OUT --password PW
  meta      FILE -o OUT [--title T] [--author A] [--subject S] [--keywords K]
            [--creator C] [--producer P] [--password PW]
  compress  FILE -o OUT [--password PW]     (lossless; the app also offers lossy)
  images    -o OUT IMAGE.jpg ... [--page fit|a4|letter] [--margin POINTS]
  --version
)";

class UsageError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

fs::path toPath(const std::string& utf8) {
    return {reinterpret_cast<const char8_t*>(utf8.c_str())};
}

// Splits arguments into positionals and --options (with values) in order of appearance.
struct Args {
    std::vector<std::string> positional;
    std::map<std::string, std::string> options;
    std::vector<std::string> flags;

    [[nodiscard]] std::optional<std::string> get(const std::string& key) const {
        auto it = options.find(key);
        return it == options.end() ? std::nullopt : std::optional(it->second);
    }
    [[nodiscard]] std::string require(const std::string& key) const {
        auto v = get(key);
        if (!v) {
            throw UsageError("missing " + key);
        }
        return *v;
    }
    [[nodiscard]] bool has(const std::string& flag) const {
        return std::ranges::find(flags, flag) != flags.end();
    }
    [[nodiscard]] const std::string& pos(std::size_t i, const char* what) const {
        if (i >= positional.size()) {
            throw UsageError(std::string("missing ") + what);
        }
        return positional[i];
    }
};

constexpr std::array<std::string_view, 17> kValueOptions = {
    "-o",         "-d",        "--password", "--pages", "--every",  "--ranges",
    "--name",     "--user",    "--owner",    "--title", "--author", "--subject",
    "--keywords", "--creator", "--producer", "--page",  "--margin"};

Args parse(const std::vector<std::string>& raw) {
    Args a;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const auto& s = raw[i];
        if (std::ranges::find(kValueOptions, s) != kValueOptions.end()) {
            if (i + 1 >= raw.size()) {
                throw UsageError(s + " needs a value");
            }
            a.options[s] = raw[++i];
        } else if (s.starts_with("--") || (s.size() == 2 && s[0] == '-' && s != "-")) {
            a.flags.push_back(s);
        } else {
            a.positional.push_back(s);
        }
    }
    return a;
}

void printWarnings(const OpResult& r) {
    for (const auto& w : r.warnings) {
        std::cerr << "warning: " << w << '\n';
    }
}

void done(const OpResult& r) {
    printWarnings(r);
    for (const auto& p : r.outputs) {
        std::cout << pathToUtf8(p) << '\n';
    }
}

SourceDoc input(const Args& a, std::size_t index = 0) {
    return {0, toPath(a.pos(index, "input FILE")), a.get("--password").value_or("")};
}

int cmdInfo(const Args& a) {
    auto info = readDocumentInfo(input(a));
    std::cout << "Pages:      " << info.pageCount << '\n'
              << "PDF version: " << info.pdfVersion << '\n'
              << "Encrypted:  " << (info.encrypted ? "yes" : "no") << '\n';
    const std::array<std::pair<const char*, const std::string*>, 8> fields = {
        {{"Title", &info.metadata.title},
         {"Author", &info.metadata.author},
         {"Subject", &info.metadata.subject},
         {"Keywords", &info.metadata.keywords},
         {"Creator", &info.metadata.creator},
         {"Producer", &info.metadata.producer},
         {"Created", &info.metadata.creationDate},
         {"Modified", &info.metadata.modDate}}};
    for (const auto& [label, value] : fields) {
        if (!value->empty()) {
            std::cout << label << ": "
                      << std::string(11 - std::char_traits<char>::length(label), ' ') << *value
                      << '\n';
        }
    }
    for (const auto& w : info.warnings) {
        std::cerr << "warning: " << w << '\n';
    }
    return kExitOk;
}

int cmdMerge(const std::vector<std::string>& raw) {
    std::vector<MergeInput> inputs;
    std::optional<fs::path> out;
    MergeOptions options;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        const auto& s = raw[i];
        auto value = [&]() -> const std::string& {
            if (i + 1 >= raw.size()) {
                throw UsageError(s + " needs a value");
            }
            return raw[++i];
        };
        if (s == "-o") {
            out = toPath(value());
        } else if (s == "--bookmarks") {
            options.bookmarkPerFile = true;
        } else if (s == "--pages" || s == "--password") {
            if (inputs.empty()) {
                throw UsageError(s + " must follow a FILE");
            }
            (s == "--pages" ? inputs.back().range : inputs.back().doc.password) = value();
        } else if (s.starts_with("-")) {
            throw UsageError("unknown option " + s);
        } else {
            inputs.push_back({{0, toPath(s), {}}, {}});
        }
    }
    if (!out) {
        throw UsageError("missing -o");
    }
    done(mergeDocuments(inputs, *out, options));
    return kExitOk;
}

int cmdSplit(const Args& a) {
    SplitOptions o;
    if (auto every = a.get("--every")) {
        o.mode = SplitMode::EveryNPages;
        o.everyN = std::stoi(*every);
    } else if (auto ranges = a.get("--ranges")) {
        o.mode = SplitMode::Ranges;
        const std::string& r = *ranges;
        for (std::size_t start = 0;;) {
            const auto end = r.find(';', start);
            o.ranges.push_back(r.substr(start, end - start));
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
    }
    if (auto name = a.get("--name")) {
        o.nameTemplate = *name;
    }
    o.overwrite = a.has("--overwrite");
    done(splitDocument(input(a), toPath(a.require("-d")), o));
    return kExitOk;
}

int cmdEncrypt(const Args& a) {
    Encryption e;
    e.userPassword = a.get("--user").value_or("");
    e.ownerPassword = a.get("--owner").value_or("");
    e.permissions.print = !a.has("--no-print");
    e.permissions.extract = !a.has("--no-copy");
    e.permissions.modify = !a.has("--no-modify");
    e.permissions.assemble = !a.has("--no-modify");
    done(encryptDocument(input(a), e, toPath(a.require("-o"))));
    return kExitOk;
}

int cmdMeta(const Args& a) {
    const auto source = input(a);
    auto m = readDocumentInfo(source).metadata;
    const std::array<std::pair<const char*, std::string*>, 6> fields = {
        {{"--title", &m.title},
         {"--author", &m.author},
         {"--subject", &m.subject},
         {"--keywords", &m.keywords},
         {"--creator", &m.creator},
         {"--producer", &m.producer}}};
    for (const auto& [key, field] : fields) {
        if (auto v = a.get(key)) {
            *field = *v;
        }
    }
    done(writeMetadata(source, m, toPath(a.require("-o"))));
    return kExitOk;
}

int cmdCompress(const Args& a) {
    auto report = compressDocument(input(a), toPath(a.require("-o")));
    done(report.result);
    std::cerr << report.bytesBefore << " -> " << report.bytesAfter << " bytes\n";
    return kExitOk;
}

int cmdImages(const Args& a) {
    if (a.positional.empty()) {
        throw UsageError("missing IMAGE files");
    }
    std::vector<ImageInput> images;
    for (const auto& file : a.positional) {
        std::ifstream in(toPath(file), std::ios::binary);
        if (!in) {
            throw PdfError(ErrorKind::Io, "Cannot open " + file);
        }
        images.push_back({ImageInput::Kind::Jpeg, {std::istreambuf_iterator<char>(in), {}}});
    }
    ImagesToPdfOptions o;
    const auto page = a.get("--page").value_or("fit");
    if (page == "a4") {
        o.pageSize = PageSize::A4;
    } else if (page == "letter") {
        o.pageSize = PageSize::Letter;
    } else if (page != "fit") {
        throw UsageError("--page must be fit, a4 or letter");
    }
    if (auto m = a.get("--margin")) {
        o.marginPoints = std::stod(*m);
    }
    done(imagesToPdf(images, toPath(a.require("-o")), o));
    return kExitOk;
}

int run(const std::vector<std::string>& raw) {
    if (raw.empty() || raw[0] == "-h" || raw[0] == "--help" || raw[0] == "help") {
        std::cout << kUsage;
        return raw.empty() ? kExitUsage : kExitOk;
    }
    const std::string& cmd = raw[0];
    const std::vector<std::string> rest(raw.begin() + 1, raw.end());
    if (cmd == "--version") {
        std::cout << "pdfh 0.1.0 (qpdf " << qpdfVersion() << ")\n";
        return kExitOk;
    }
    if (cmd == "merge") {
        return cmdMerge(rest);
    }
    const Args a = parse(rest);
    if (cmd == "info") {
        return cmdInfo(a);
    }
    if (cmd == "extract") {
        done(extractPages(input(a), a.pos(1, "RANGE"), toPath(a.require("-o"))));
        return kExitOk;
    }
    if (cmd == "delete") {
        done(deletePages(input(a), a.pos(1, "RANGE"), toPath(a.require("-o"))));
        return kExitOk;
    }
    if (cmd == "rotate") {
        const int degrees = std::stoi(a.pos(1, "DEGREES"));
        const std::string range = a.positional.size() > 2 ? a.positional[2] : "";
        done(rotatePages(input(a), range, degrees, toPath(a.require("-o"))));
        return kExitOk;
    }
    if (cmd == "split") {
        return cmdSplit(a);
    }
    if (cmd == "encrypt") {
        return cmdEncrypt(a);
    }
    if (cmd == "decrypt") {
        done(decryptDocument(input(a), toPath(a.require("-o"))));
        return kExitOk;
    }
    if (cmd == "meta") {
        return cmdMeta(a);
    }
    if (cmd == "compress") {
        return cmdCompress(a);
    }
    if (cmd == "images") {
        return cmdImages(a);
    }
    throw UsageError("unknown command '" + cmd + "'");
}

} // namespace

std::vector<std::string> utf8Args(int argc, char** argv) {
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int n = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &n);
    std::vector<std::string> out;
    for (int i = 1; i < n; ++i) {
        const int len = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(static_cast<std::size_t>(len > 0 ? len - 1 : 0), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, s.data(), len, nullptr, nullptr);
        out.push_back(std::move(s));
    }
    LocalFree(wide);
    return out;
#else
    return {argv + 1, argv + argc};
#endif
}

int runCli(const std::vector<std::string>& args) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    try {
        return run(args);
    } catch (const UsageError& e) {
        std::cerr << "pdfh: " << e.what() << "\n\n" << kUsage;
        return kExitUsage;
    } catch (const PdfError& e) {
        std::cerr << "pdfh: " << e.what() << '\n';
        if (e.kind() == ErrorKind::PasswordRequired) {
            std::cerr << "(pass the password with --password)\n";
        }
        return kExitError;
    } catch (const std::invalid_argument&) {
        std::cerr << "pdfh: expected a number\n";
        return kExitUsage;
    } catch (const std::exception& e) {
        std::cerr << "pdfh: " << e.what() << '\n';
        return kExitError;
    }
}
