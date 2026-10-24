#include "core/ops/ImagesToPdf.hpp"

#include "core/Document.hpp"

#include <algorithm>
#include <cstdint>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <sstream>

namespace pdfcore {

namespace {

constexpr double kDefaultDpi = 96.0;
constexpr double kPointsPerInch = 72.0;
constexpr double kA4Width = 595.276;
constexpr double kA4Height = 841.89;
constexpr double kLetterWidth = 612.0;
constexpr double kLetterHeight = 792.0;

int be16(std::string_view d, std::size_t pos) {
    return (static_cast<unsigned char>(d[pos]) << 8) | static_cast<unsigned char>(d[pos + 1]);
}

std::string fmt(double v) {
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s.setf(std::ios::fixed);
    s.precision(4);
    s << v;
    return s.str();
}

} // namespace

std::optional<JpegInfo> parseJpegHeader(std::string_view d) {
    if (d.size() < 4 || static_cast<unsigned char>(d[0]) != 0xFF ||
        static_cast<unsigned char>(d[1]) != 0xD8) {
        return std::nullopt;
    }
    JpegInfo info;
    std::size_t pos = 2;
    while (pos + 4 <= d.size()) {
        if (static_cast<unsigned char>(d[pos]) != 0xFF) {
            return std::nullopt;
        }
        const auto marker = static_cast<unsigned char>(d[pos + 1]);
        if (marker == 0xFF) { // fill byte
            ++pos;
            continue;
        }
        if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
            pos += 2; // markers without a length
            continue;
        }
        const auto length = static_cast<std::size_t>(be16(d, pos + 2));
        if (length < 2 || pos + 2 + length > d.size()) {
            return std::nullopt;
        }
        const std::string_view seg = d.substr(pos + 4, length - 2);
        if (marker == 0xE0 && seg.size() >= 12 &&
            seg.substr(0, 5) == std::string_view("JFIF\0", 5)) {
            const int units = static_cast<unsigned char>(seg[7]);
            const int x = be16(seg, 8);
            const int y = be16(seg, 10);
            if (units == 1 && x > 0 && y > 0) {
                info.dpiX = x;
                info.dpiY = y;
            } else if (units == 2 && x > 0 && y > 0) { // dots per cm
                info.dpiX = x * 2.54;
                info.dpiY = y * 2.54;
            }
        }
        // SOF0..SOF15 except DHT (C4), JPG (C8), DAC (CC)
        if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 &&
            marker != 0xCC) {
            if (seg.size() < 6) {
                return std::nullopt;
            }
            if (static_cast<unsigned char>(seg[0]) != 8) {
                return std::nullopt; // 12-bit JPEGs are not allowed in PDF DCTDecode
            }
            info.height = be16(seg, 1);
            info.width = be16(seg, 3);
            info.components = static_cast<unsigned char>(seg[5]);
            if (info.width <= 0 || info.height <= 0) {
                return std::nullopt;
            }
            return info;
        }
        if (marker == 0xDA) { // start of scan before any SOF: malformed
            return std::nullopt;
        }
        pos += 2 + length;
    }
    return std::nullopt;
}

OpResult imagesToPdf(const std::vector<ImageInput>& images, const std::filesystem::path& output,
                     const ImagesToPdfOptions& options, const JobContext& ctx) {
    if (images.empty()) {
        throw PdfError(ErrorKind::InvalidArgument, "Add at least one image");
    }
    const int n = static_cast<int>(images.size());
    OpResult result;
    AtomicOutput out(output);

    auto pdf = QPDF::create();
    pdf->setSuppressWarnings(true);
    pdf->emptyPDF();
    QPDFPageDocumentHelper pages(*pdf);

    ctx.report(0, 2 * n);
    for (int i = 0; i < n; ++i) {
        ctx.checkCancel();
        const auto& img = images[static_cast<std::size_t>(i)];
        const std::string label = "Image " + std::to_string(i + 1);

        int width = img.width;
        int height = img.height;
        int components = img.components;
        double dpi = img.dpi;
        QPDFObjectHandle stream;
        if (img.kind == ImageInput::Kind::Jpeg) {
            auto info = parseJpegHeader(img.data);
            if (!info) {
                throw PdfError(ErrorKind::InvalidArgument, label + " is not a readable JPEG");
            }
            if (info->components != 1 && info->components != 3) {
                throw PdfError(ErrorKind::InvalidArgument,
                               label + " is a CMYK JPEG; convert it to pixels first");
            }
            width = info->width;
            height = info->height;
            components = info->components;
            if (dpi <= 0) {
                dpi = info->dpiX;
            }
            stream = pdf->newStream(img.data);
            stream.getDict().replaceKey("/Filter", QPDFObjectHandle::newName("/DCTDecode"));
        } else {
            if (width <= 0 || height <= 0 || (components != 1 && components != 3) ||
                img.data.size() != static_cast<std::size_t>(width) *
                                       static_cast<std::size_t>(height) *
                                       static_cast<std::size_t>(components)) {
                throw PdfError(ErrorKind::InvalidArgument, label + " has inconsistent pixel data");
            }
            stream = pdf->newStream(img.data); // QPDFWriter compresses it with Flate
        }
        if (dpi <= 0) {
            dpi = kDefaultDpi;
        }
        auto dict = stream.getDict();
        dict.replaceKey("/Type", QPDFObjectHandle::newName("/XObject"));
        dict.replaceKey("/Subtype", QPDFObjectHandle::newName("/Image"));
        dict.replaceKey("/Width", QPDFObjectHandle::newInteger(width));
        dict.replaceKey("/Height", QPDFObjectHandle::newInteger(height));
        dict.replaceKey("/BitsPerComponent", QPDFObjectHandle::newInteger(8));
        dict.replaceKey("/ColorSpace",
                        QPDFObjectHandle::newName(components == 1 ? "/DeviceGray" : "/DeviceRGB"));

        // Natural size of the image in points.
        const double imgW = width * kPointsPerInch / dpi;
        const double imgH = height * kPointsPerInch / dpi;
        double pageW = imgW;
        double pageH = imgH;
        double margin = 0;
        if (options.pageSize != PageSize::FitImage) {
            pageW = options.pageSize == PageSize::A4 ? kA4Width : kLetterWidth;
            pageH = options.pageSize == PageSize::A4 ? kA4Height : kLetterHeight;
            const bool landscape = options.orientation == Orientation::Landscape ||
                                   (options.orientation == Orientation::Auto && imgW > imgH);
            if (landscape) {
                std::swap(pageW, pageH);
            }
            margin = std::clamp(options.marginPoints, 0.0, std::min(pageW, pageH) / 2 - 1);
        }
        const double boxW = pageW - 2 * margin;
        const double boxH = pageH - 2 * margin;
        const double scale = std::min({1.0, boxW / imgW, boxH / imgH});
        const double drawW = imgW * scale;
        const double drawH = imgH * scale;
        const double x = (pageW - drawW) / 2;
        const double y = (pageH - drawH) / 2;

        auto xobjects = QPDFObjectHandle::newDictionary();
        xobjects.replaceKey("/Im0", stream);
        auto resources = QPDFObjectHandle::newDictionary();
        resources.replaceKey("/XObject", xobjects);

        auto page = pdf->makeIndirectObject(QPDFObjectHandle::newDictionary());
        page.replaceKey("/Type", QPDFObjectHandle::newName("/Page"));
        page.replaceKey("/MediaBox",
                        QPDFObjectHandle::parse("[0 0 " + fmt(pageW) + " " + fmt(pageH) + "]"));
        page.replaceKey("/Resources", resources);
        page.replaceKey("/Contents", pdf->newStream("q " + fmt(drawW) + " 0 0 " + fmt(drawH) + " " +
                                                    fmt(x) + " " + fmt(y) + " cm /Im0 Do Q"));
        pages.addPage(QPDFPageObjectHelper(page), false);
        ctx.report(i + 1, 2 * n);
    }

    auto validation = writeAndValidate(
        *pdf, out, options.write, n, reopenPasswordFor(options.write, {}), ctx.slice(n, n, 2 * n));
    appendWarnings(result.warnings, takeWarnings(*pdf));
    appendWarnings(result.warnings, validation.warnings);
    pdf.reset();
    out.commit();
    result.outputs.push_back(output);
    return result;
}

} // namespace pdfcore
