#include "core/ops/Metadata.hpp"

#include "core/Document.hpp"
#include "core/ops/Rewrite.hpp"

#include <array>
#include <qpdf/QPDF.hh>
#include <qpdf/QUtil.hh>
#include <utility>

namespace pdfcore {

namespace {

using Field = std::string Metadata::*;

constexpr std::array<std::pair<const char*, Field>, 8> kFields{{
    {"/Title", &Metadata::title},
    {"/Author", &Metadata::author},
    {"/Subject", &Metadata::subject},
    {"/Keywords", &Metadata::keywords},
    {"/Creator", &Metadata::creator},
    {"/Producer", &Metadata::producer},
    {"/CreationDate", &Metadata::creationDate},
    {"/ModDate", &Metadata::modDate},
}};

constexpr std::size_t kEditableFields = 6; // the first six; dates are managed

} // namespace

DocumentInfo readDocumentInfo(const SourceDoc& source) {
    auto doc = openDocument(source.path, source.password);
    DocumentInfo info;
    info.pageCount = doc.pageCount;
    info.encrypted = doc.encrypted;
    info.pdfVersion = doc.pdf->getPDFVersion();
    info.hasXmpMetadata = doc.pdf->getRoot().getKey("/Metadata").isStream();
    auto dict = doc.pdf->getTrailer().getKey("/Info");
    if (dict.isDictionary()) {
        for (const auto& [key, field] : kFields) {
            auto value = dict.getKey(key);
            if (value.isString()) {
                info.metadata.*field = value.getUTF8Value();
            }
        }
    }
    info.warnings = doc.warnings;
    appendWarnings(info.warnings, takeWarnings(*doc.pdf));
    return info;
}

OpResult writeMetadata(const SourceDoc& source, const Metadata& metadata,
                       const std::filesystem::path& output, const JobContext& ctx) {
    return rewriteDocument(
        source, output, WriteOptions{}, ctx,
        [&metadata](QPDF& pdf, int /*pages*/, OpResult& result, const JobContext& /*pageCtx*/) {
            auto trailer = pdf.getTrailer();
            auto dict = trailer.getKey("/Info");
            if (!dict.isDictionary()) {
                dict = pdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
                trailer.replaceKey("/Info", dict);
            }
            for (std::size_t i = 0; i < kEditableFields; ++i) {
                const auto& [key, field] = kFields[i];
                const auto& value = metadata.*field;
                if (value.empty()) {
                    dict.removeKey(key);
                } else {
                    dict.replaceKey(key, QPDFObjectHandle::newUnicodeString(value));
                }
            }
            dict.replaceKey("/ModDate", QPDFObjectHandle::newString(QUtil::qpdf_time_to_pdf_time(
                                            QUtil::get_current_qpdf_time())));
            if (pdf.getRoot().getKey("/Metadata").isStream()) {
                result.warnings.emplace_back(
                    "This file also stores XMP metadata, which was left unchanged. Some "
                    "viewers show the XMP values instead of the ones you edited.");
            }
        });
}

} // namespace pdfcore
