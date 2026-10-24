#include "services/QImageRecoder.hpp"

#include <QBuffer>
#include <QImage>
#include <QImageWriter>

std::optional<pdfcore::EncodedJpeg> QImageRecoder::recode(const pdfcore::RawImage& raw) {
    const auto format = raw.components == 1 ? QImage::Format_Grayscale8 : QImage::Format_RGB888;
    const qsizetype stride = static_cast<qsizetype>(raw.width) * raw.components;
    // Wraps the pixel buffer without copying; the copy happens on scale/convert.
    QImage image(reinterpret_cast<const uchar*>(raw.pixels.data()), raw.width, raw.height, stride,
                 format);
    if (image.isNull()) {
        return std::nullopt;
    }
    if (raw.mayResize && std::max(raw.width, raw.height) > m_maxEdge) {
        image = image.scaled(m_maxEdge, m_maxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, "jpeg");
    writer.setQuality(m_quality);
    writer.setOptimizedWrite(true);
    if (!writer.write(image)) {
        return std::nullopt;
    }
    return pdfcore::EncodedJpeg{image.width(), image.height(), bytes.toStdString()};
}
