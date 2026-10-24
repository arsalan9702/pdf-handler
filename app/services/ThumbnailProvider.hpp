#pragma once

#include "core/Types.hpp"

#include <QCache>
#include <QHash>
#include <QImage>
#include <QMutex>
#include <QQuickAsyncImageProvider>
#include <QThreadPool>
#include <memory>

class QPdfDocument;

// Serves "image://thumb/<docId>/<pageIndex>" from a cache of low-DPI page renders.
// Rendering runs on one dedicated thread (PDFium is single-threaded anyway), so the GUI thread
// never blocks and scrolling a 2,000-page grid only renders what is visible.
class ThumbnailProvider : public QQuickAsyncImageProvider {
  public:
    ThumbnailProvider();
    ~ThumbnailProvider() override;

    // The process-wide provider; created by main() and handed to the QML engine.
    static ThumbnailProvider* instance();

    void registerDocument(pdfcore::DocId id, const QString& path, const QString& password);
    void unregisterDocument(pdfcore::DocId id);

    QQuickImageResponse* requestImageResponse(const QString& id,
                                              const QSize& requestedSize) override;

    // Called on the render thread only.
    QImage render(pdfcore::DocId id, int page, const QSize& requestedSize);

  private:
    struct Source {
        QString path;
        QString password;
        std::unique_ptr<QPdfDocument> document; // opened lazily on the render thread
    };

    QMutex m_mutex;
    QHash<pdfcore::DocId, std::shared_ptr<Source>> m_sources;
    QCache<QString, QImage> m_cache; // cost in KiB
    QThreadPool m_pool;
};
