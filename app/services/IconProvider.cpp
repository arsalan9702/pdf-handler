#include "services/IconProvider.hpp"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

IconProvider::IconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage IconProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    const auto parts = id.split(u'/');
    const QString name = parts.value(0);
    const QString color = parts.value(1, QStringLiteral("000000"));
    const QSize out =
        requestedSize.isValid() && !requestedSize.isEmpty() ? requestedSize : QSize(24, 24);
    if (size != nullptr) {
        *size = out;
    }

    QFile file(QStringLiteral(":/qt/qml/PdfHandler/qml/icons/%1.svg").arg(name));
    QImage image(out, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Missing icon: %s", qPrintable(name));
        return image;
    }
    QByteArray svg = file.readAll();
    svg.replace("currentColor", "#" + color.toLatin1());
    QSvgRenderer renderer(svg);
    QPainter painter(&image);
    renderer.render(&painter);
    return image;
}
