#pragma once

#include "core/ops/Compress.hpp"

// Lossy re-encoder for the compression pass: smooth downscale to `maxEdge` pixels on the long
// side (only when allowed), then JPEG at `quality`.
class QImageRecoder : public pdfcore::ImageRecoder {
  public:
    QImageRecoder(int quality, int maxEdge) : m_quality(quality), m_maxEdge(maxEdge) {}
    std::optional<pdfcore::EncodedJpeg> recode(const pdfcore::RawImage& image) override;

  private:
    int m_quality;
    int m_maxEdge;
};
