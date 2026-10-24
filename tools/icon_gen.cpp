// Renders the SVG app icon into the per-platform formats at build time:
//   <out>/app.ico                      Windows (PNG-compressed entries, 16..256 px)
//   <out>/app.iconset/icon_*.png       macOS (turned into app.icns by iconutil)
//   <out>/png/<size>.png               Linux / AppImage

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <cstdio>
#include <vector>

namespace {

QImage render(QSvgRenderer& svg, int size) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    svg.render(&painter);
    return image;
}

QByteArray png(const QImage& image) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

void put16(QByteArray& b, int v) {
    b.append(char(v & 0xFF));
    b.append(char((v >> 8) & 0xFF));
}

void put32(QByteArray& b, quint32 v) {
    put16(b, int(v & 0xFFFF));
    put16(b, int(v >> 16));
}

} // namespace

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    if (argc != 3) {
        std::fprintf(stderr, "usage: icon_gen <icon.svg> <out-dir>\n");
        return 2;
    }
    QSvgRenderer svg(QString::fromLocal8Bit(argv[1]));
    if (!svg.isValid()) {
        std::fprintf(stderr, "icon_gen: cannot read %s\n", argv[1]);
        return 1;
    }
    const QDir out(QString::fromLocal8Bit(argv[2]));
    out.mkpath(QStringLiteral("png"));
    out.mkpath(QStringLiteral("app.iconset"));

    for (int size : {16, 24, 32, 48, 64, 128, 256, 512, 1024}) {
        render(svg, size).save(out.filePath(QStringLiteral("png/%1.png").arg(size)));
    }

    // ICO: header, directory, then PNG payloads (valid since Windows Vista).
    const std::vector<int> icoSizes{16, 24, 32, 48, 64, 128, 256};
    std::vector<QByteArray> payloads;
    for (int s : icoSizes) {
        payloads.push_back(png(render(svg, s)));
    }
    QByteArray ico;
    put16(ico, 0);
    put16(ico, 1);
    put16(ico, int(icoSizes.size()));
    quint32 offset = 6 + 16 * quint32(icoSizes.size());
    for (std::size_t i = 0; i < icoSizes.size(); ++i) {
        const int s = icoSizes[i];
        ico.append(char(s >= 256 ? 0 : s)); // 0 means 256
        ico.append(char(s >= 256 ? 0 : s));
        ico.append(char(0));
        ico.append(char(0));
        put16(ico, 1);
        put16(ico, 32);
        put32(ico, quint32(payloads[i].size()));
        put32(ico, offset);
        offset += quint32(payloads[i].size());
    }
    for (const auto& p : payloads) {
        ico.append(p);
    }
    QFile icoFile(out.filePath(QStringLiteral("app.ico")));
    if (!icoFile.open(QIODevice::WriteOnly) || icoFile.write(ico) != ico.size()) {
        return 1;
    }

    for (int s : {16, 32, 128, 256, 512}) {
        render(svg, s).save(out.filePath(QStringLiteral("app.iconset/icon_%1x%1.png").arg(s)));
        render(svg, s * 2)
            .save(out.filePath(QStringLiteral("app.iconset/icon_%1x%1@2x.png").arg(s)));
    }
    return 0;
}
