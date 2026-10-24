#pragma once

#include <QQuickImageProvider>

// Serves "image://icon/<name>/<rrggbb>": a Lucide SVG from :/icons with its currentColor
// replaced by the given colour, rendered crisply at the requested size.
class IconProvider : public QQuickImageProvider {
  public:
    IconProvider();
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
