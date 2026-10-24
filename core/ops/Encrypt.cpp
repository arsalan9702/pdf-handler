#include "core/ops/Encrypt.hpp"

#include "core/ops/Rewrite.hpp"

namespace pdfcore {

OpResult encryptDocument(const SourceDoc& source, const Encryption& encryption,
                         const std::filesystem::path& output, const JobContext& ctx) {
    if (encryption.ownerPassword.empty() && encryption.userPassword.empty()) {
        throw PdfError(ErrorKind::InvalidArgument, "Enter a password");
    }
    WriteOptions options;
    options.encryptionMode = EncryptionMode::Set;
    options.encryption = encryption;
    // Without an owner password anyone could lift the restrictions; reuse the user password.
    if (options.encryption->ownerPassword.empty()) {
        options.encryption->ownerPassword = options.encryption->userPassword;
    }
    return rewriteDocument(source, output, options, ctx, nullptr);
}

OpResult decryptDocument(const SourceDoc& source, const std::filesystem::path& output,
                         const JobContext& ctx) {
    WriteOptions options;
    options.encryptionMode = EncryptionMode::Remove;
    return rewriteDocument(source, output, options, ctx, nullptr);
}

} // namespace pdfcore
