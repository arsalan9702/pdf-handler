#include "core/ops/Compress.hpp"

#include "core/ops/Rewrite.hpp"

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFPageObjectHelper.hh>
#include <set>

namespace pdfcore {

namespace {

constexpr int kMinDimension = 64;          // smaller images are not worth re-encoding
constexpr std::size_t kMinRawBytes = 8192; // nor are tiny streams

// Components for colour spaces we can re-encode without changing how colours are interpreted.
std::optional<int> supportedComponents(const QPDFObjectHandle& cs) {
    if (cs.isName()) {
        if (cs.getName() == "/DeviceRGB") {
            return 3;
        }
        if (cs.getName() == "/DeviceGray") {
            return 1;
        }
        return std::nullopt;
    }
    // [/ICCBased stream] with N = 1 or 3; the profile stays attached, only samples change.
    if (cs.isArray() && cs.getArrayNItems() == 2 &&
        cs.getArrayItem(0).isNameAndEquals("/ICCBased")) {
        auto profile = cs.getArrayItem(1);
        if (!profile.isStream()) {
            return std::nullopt;
        }
        auto n = profile.getDict().getKey("/N");
        if (n.isInteger() && (n.getIntValue() == 1 || n.getIntValue() == 3)) {
            return static_cast<int>(n.getIntValue());
        }
    }
    return std::nullopt;
}

bool hasUnsupportedFilter(const QPDFObjectHandle& dict) {
    auto filter = dict.getKey("/Filter");
    std::vector<QPDFObjectHandle> filters;
    if (filter.isName()) {
        filters.push_back(filter);
    } else if (filter.isArray()) {
        filters = filter.getArrayAsVector();
    } else if (!filter.isNull()) {
        return true;
    }
    for (const auto& f : filters) {
        if (!f.isName()) {
            return true;
        }
        const auto name = f.getName();
        if (name == "/JBIG2Decode" || name == "/JPXDecode" || name == "/CCITTFaxDecode" ||
            name == "/Crypt") {
            return true;
        }
    }
    return false;
}

// Returns the decoded image if it is safe to re-encode, otherwise nullopt.
std::optional<RawImage> eligibleImage(QPDFObjectHandle& image) {
    auto dict = image.getDict();
    if (!dict.getKey("/Subtype").isNameAndEquals("/Image")) {
        return std::nullopt;
    }
    if (dict.getKey("/ImageMask").isBool() && dict.getKey("/ImageMask").getBoolValue()) {
        return std::nullopt;
    }
    if (dict.hasKey("/Mask") || dict.hasKey("/Decode") || dict.hasKey("/SMaskInData") ||
        dict.hasKey("/Alternates") || dict.hasKey("/OC")) {
        return std::nullopt;
    }
    auto bpc = dict.getKey("/BitsPerComponent");
    auto w = dict.getKey("/Width");
    auto h = dict.getKey("/Height");
    if (!bpc.isInteger() || bpc.getIntValue() != 8 || !w.isInteger() || !h.isInteger()) {
        return std::nullopt;
    }
    const auto components = supportedComponents(dict.getKey("/ColorSpace"));
    if (!components || hasUnsupportedFilter(dict)) {
        return std::nullopt;
    }
    RawImage raw;
    raw.width = static_cast<int>(w.getIntValue());
    raw.height = static_cast<int>(h.getIntValue());
    raw.components = *components;
    if (raw.width < kMinDimension || raw.height < kMinDimension) {
        return std::nullopt;
    }
    const auto expected = static_cast<std::size_t>(raw.width) *
                          static_cast<std::size_t>(raw.height) *
                          static_cast<std::size_t>(raw.components);
    if (expected < kMinRawBytes) {
        return std::nullopt;
    }
    std::shared_ptr<Buffer> data;
    try {
        data = image.getStreamData(qpdf_dl_all);
    } catch (const std::exception&) {
        return std::nullopt; // cannot decode: leave it alone
    }
    if (!data || data->getSize() != expected) {
        return std::nullopt;
    }
    raw.pixels.assign(reinterpret_cast<const char*>(data->getBuffer()), data->getSize());
    // A soft mask is a separate image sized for this one; do not change dimensions under it.
    raw.mayResize = !dict.hasKey("/SMask");
    return raw;
}

} // namespace

CompressReport compressDocument(const SourceDoc& source, const std::filesystem::path& output,
                                const CompressOptions& options, const JobContext& ctx) {
    CompressReport report;
    std::error_code ec;
    report.bytesBefore = std::filesystem::file_size(source.path, ec);

    WriteOptions write = options.write;
    write.objectStreams = ObjectStreams::Generate;
    write.recompressFlate = true;

    report.result = rewriteDocument(
        source, output, write, ctx,
        [&](QPDF& pdf, int pageCount, OpResult& result, const JobContext& pageCtx) {
            QPDFPageDocumentHelper pages(pdf);
            pages.removeUnreferencedResources();
            if (options.recoder == nullptr) {
                return;
            }
            std::set<QPDFObjGen> seen;
            int done = 0;
            for (auto& page : pages.getAllPages()) {
                pageCtx.checkCancel();
                page.forEachImage(true, [&](QPDFObjectHandle& image, QPDFObjectHandle&,
                                            const std::string&) {
                    if (!seen.insert(image.getObjGen()).second) {
                        return;
                    }
                    auto raw = eligibleImage(image);
                    if (!raw) {
                        ++report.imagesSkipped;
                        return;
                    }
                    const auto oldSize = image.getRawStreamData()->getSize();
                    auto jpeg = options.recoder->recode(*raw);
                    const bool valid = jpeg && !jpeg->data.empty() && jpeg->width > 0 &&
                                       jpeg->height > 0 &&
                                       (raw->mayResize ||
                                        (jpeg->width == raw->width && jpeg->height == raw->height));
                    if (!valid || jpeg->data.size() >= oldSize) {
                        ++report.imagesSkipped;
                        return;
                    }
                    image.replaceStreamData(jpeg->data, QPDFObjectHandle::newName("/DCTDecode"),
                                            QPDFObjectHandle::newNull());
                    auto dict = image.getDict();
                    dict.replaceKey("/Width", QPDFObjectHandle::newInteger(jpeg->width));
                    dict.replaceKey("/Height", QPDFObjectHandle::newInteger(jpeg->height));
                    ++report.imagesReplaced;
                });
                pageCtx.report(++done, pageCount);
            }
            if (report.imagesReplaced > 0) {
                result.warnings.emplace_back(std::to_string(report.imagesReplaced) +
                                             " image(s) were re-encoded at lower quality.");
            }
        });

    report.bytesAfter = std::filesystem::file_size(output, ec);
    if (report.bytesAfter >= report.bytesBefore && report.bytesBefore > 0) {
        report.result.warnings.emplace_back(
            "The file could not be made smaller; it was already well compressed.");
    }
    return report;
}

} // namespace pdfcore
