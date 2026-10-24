#pragma once

#include "core/SafeWriter.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pdfcore {

struct JpegInfo {
    int width = 0;
    int height = 0;
    int components = 0; // 1 = gray, 3 = colour, 4 = CMYK
    double dpiX = 0;    // 0 when the file does not say
    double dpiY = 0;
};

// Reads dimensions, component count and density from a JPEG's headers without decoding it.
std::optional<JpegInfo> parseJpegHeader(std::string_view data);

struct ImageInput {
    enum class Kind {
        Jpeg,   // `data` is a JPEG file, embedded as-is (no quality loss)
        Pixels, // `data` is 8-bit gray or RGB samples, width * height * components bytes
    };
    Kind kind = Kind::Pixels;
    std::string data;
    int width = 0;      // Pixels only
    int height = 0;     // Pixels only
    int components = 0; // Pixels only: 1 or 3
    double dpi = 0;     // 0 = use the JPEG's density, else 96
};

enum class PageSize { FitImage, A4, Letter };
enum class Orientation { Auto, Portrait, Landscape };

struct ImagesToPdfOptions {
    PageSize pageSize = PageSize::FitImage;
    Orientation orientation = Orientation::Auto;
    double marginPoints = 0; // ignored for FitImage
    WriteOptions write;
};

// One page per image, image scaled to fit (never upscaled beyond the page) and centred.
// CMYK JPEGs are rejected with InvalidArgument; decode them to pixels first.
OpResult imagesToPdf(const std::vector<ImageInput>& images, const std::filesystem::path& output,
                     const ImagesToPdfOptions& options = {}, const JobContext& ctx = {});

} // namespace pdfcore
