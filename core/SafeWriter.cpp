#include "core/SafeWriter.hpp"

#include "core/Document.hpp"

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFJob.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFWriter.hh>
#include <random>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace pdfcore {

namespace {

std::string randomSuffix() {
    std::random_device rd;
    std::uniform_int_distribution<unsigned> dist(0, 0xFFFFFF);
    std::ostringstream s;
    s << std::hex << dist(rd) << dist(rd);
    return s.str();
}

void flushToDisk(const std::filesystem::path& path) {
#ifdef _WIN32
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(h);
        CloseHandle(h);
    }
#else
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }
#endif
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

} // namespace

AtomicOutput::AtomicOutput(std::filesystem::path target) : m_target(std::move(target)) {
    if (m_target.empty() || m_target.filename().empty()) {
        throw PdfError(ErrorKind::InvalidArgument, "No output file name given");
    }
    auto dir = m_target.parent_path();
    std::error_code ec;
    if (!dir.empty() && !std::filesystem::is_directory(dir, ec)) {
        throw PdfError(ErrorKind::Io, "Output folder does not exist: " + pathToUtf8(dir));
    }
    auto name = m_target.filename();
    name += ".pdfh-" + randomSuffix() + ".tmp";
    // Hidden dot-file in the same directory so the final rename stays on one filesystem.
    m_temp = dir / (std::filesystem::path(".") += name);
}

AtomicOutput::~AtomicOutput() {
    if (!m_committed) {
        std::error_code ec;
        std::filesystem::remove(m_temp, ec);
    }
}

void AtomicOutput::commit() {
    flushToDisk(m_temp);
#ifdef _WIN32
    if (MoveFileExW(m_temp.c_str(), m_target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
        throw PdfError(ErrorKind::Io, "Cannot replace " + pathToUtf8(m_target) +
                                          " (is it open in another program?)");
    }
#else
    std::error_code ec;
    std::filesystem::rename(m_temp, m_target, ec);
    if (ec) {
        throw PdfError(ErrorKind::Io, "Cannot write " + pathToUtf8(m_target) + ": " + ec.message());
    }
#endif
    m_committed = true;
}

ValidationResult validatePdf(const std::filesystem::path& path, const std::string& password,
                             std::optional<int> expectedPages) {
    const std::string name = pathToUtf8(path);
    ValidationResult result;

    std::ostringstream out;
    std::ostringstream err;
    auto logger = QPDFLogger::create();
    logger->setOutputStreams(&out, &err);

    int exitCode = 0;
    try {
        QPDFJob job;
        job.setLogger(logger);
        auto config = job.config(); // keep the shared_ptr alive; methods return raw Config*
        config->inputFile(name);
        if (!password.empty()) {
            config->password(password);
        }
        config->check()->checkConfiguration();
        job.run();
        exitCode = job.getExitCode();
    } catch (const std::exception& e) {
        throw PdfError(ErrorKind::ValidationFailed,
                       "Output failed verification, original left unchanged: " +
                           std::string(e.what()));
    }
    if (exitCode == QPDFJob::EXIT_ERROR) {
        throw PdfError(ErrorKind::ValidationFailed,
                       "Output failed verification, original left unchanged:\n" + out.str() +
                           err.str());
    }
    result.warnings = splitLines(err.str());

    try {
        auto doc = openDocument(path, password);
        result.pageCount = doc.pageCount;
    } catch (const PdfError& e) {
        throw PdfError(ErrorKind::ValidationFailed,
                       std::string("Output could not be reopened: ") + e.what());
    }
    if (expectedPages && result.pageCount != *expectedPages) {
        throw PdfError(ErrorKind::ValidationFailed,
                       "Output has " + std::to_string(result.pageCount) + " pages, expected " +
                           std::to_string(*expectedPages));
    }
    return result;
}

std::string reopenPasswordFor(const WriteOptions& options, const std::string& sourcePassword) {
    switch (options.encryptionMode) {
    case EncryptionMode::Preserve:
        return sourcePassword;
    case EncryptionMode::Remove:
        return {};
    case EncryptionMode::Set:
        return options.encryption ? options.encryption->ownerPassword : std::string{};
    }
    return {};
}

ValidationResult writeAndValidate(QPDF& pdf, AtomicOutput& out, const WriteOptions& options,
                                  int expectedPages, const std::string& reopenPassword,
                                  const JobContext& ctx, QPDF* encryptionSource) {
    ctx.checkCancel();
    const std::string tempName = pathToUtf8(out.tempPath());
    try {
        QPDFWriter writer(pdf, tempName.c_str());
        switch (options.objectStreams) {
        case ObjectStreams::Preserve:
            writer.setObjectStreamMode(qpdf_o_preserve);
            break;
        case ObjectStreams::Generate:
            writer.setObjectStreamMode(qpdf_o_generate);
            break;
        case ObjectStreams::Disable:
            writer.setObjectStreamMode(qpdf_o_disable);
            break;
        }
        writer.setLinearization(options.linearize);
        writer.setRecompressFlate(options.recompressFlate);

        switch (options.encryptionMode) {
        case EncryptionMode::Preserve:
            if (encryptionSource != nullptr) {
                writer.copyEncryptionParameters(*encryptionSource);
            } else {
                writer.setPreserveEncryption(true);
            }
            break;
        case EncryptionMode::Remove:
            writer.setPreserveEncryption(false);
            break;
        case EncryptionMode::Set: {
            if (!options.encryption || options.encryption->ownerPassword.empty()) {
                throw PdfError(ErrorKind::InvalidArgument, "An owner password is required");
            }
            const auto& enc = *options.encryption;
            const auto& p = enc.permissions;
            const qpdf_r3_print_e print = !p.print             ? qpdf_r3p_none
                                          : p.highQualityPrint ? qpdf_r3p_full
                                                               : qpdf_r3p_low;
            writer.setR6EncryptionParameters(enc.userPassword.c_str(), enc.ownerPassword.c_str(),
                                             p.accessibility, p.extract, p.assemble, p.annotate,
                                             p.fillForms, p.modify, print, true);
            break;
        }
        }

        if (ctx.cancel != nullptr || ctx.progress) {
            writer.registerProgressReporter(
                std::make_shared<QPDFWriter::FunctionProgressReporter>([&ctx](int percent) {
                    ctx.checkCancel();
                    ctx.report(percent, 100);
                }));
        }
        writer.write();
    } catch (...) {
        rethrowAsPdfError("Writing " + pathToUtf8(out.targetPath()), false);
    }
    ctx.checkCancel();
    return validatePdf(out.tempPath(), reopenPassword, expectedPages);
}

} // namespace pdfcore
