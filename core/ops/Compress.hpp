#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace pdfcore {

// 8 bits per component, row-major, no row padding. components is 1 (gray) or 3 (RGB).
struct RawImage {
    int width = 0;
    int height = 0;
    int components = 0;
    std::string pixels;
    bool mayResize = true; // false when a soft mask must keep matching the image
};

struct EncodedJpeg {
    int width = 0;
    int height = 0;
    std::string data; // baseline or progressive JPEG with the same component count
};

// Lossy image re-encoder, supplied by the caller (the app implements it with QImage) so core
// stays free of image libraries. Return nullopt to leave an image untouched.
class ImageRecoder {
  public:
    virtual ~ImageRecoder() = default;
    virtual std::optional<EncodedJpeg> recode(const RawImage& image) = 0;
};

struct CompressOptions {
    ImageRecoder* recoder = nullptr; // nullptr = lossless only
    WriteOptions write;              // object streams/recompression are forced on
};

struct CompressReport {
    OpResult result;
    int imagesReplaced = 0;
    int imagesSkipped = 0; // unsupported or not worth replacing
    std::uintmax_t bytesBefore = 0;
    std::uintmax_t bytesAfter = 0;
};

// Lossless pass: object streams, Flate recompression, unreferenced resources removed.
// Lossy pass (when a recoder is given): only 8-bit DeviceGray/DeviceRGB (or ICCBased N=1/3)
// images without /Mask, /Decode, or JBIG2/JPX/CCITT data are offered to the recoder, and an
// image is replaced only if the result is smaller.
CompressReport compressDocument(const SourceDoc& source, const std::filesystem::path& output,
                                const CompressOptions& options = {}, const JobContext& ctx = {});

} // namespace pdfcore
