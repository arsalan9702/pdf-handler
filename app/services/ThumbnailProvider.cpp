#include "services/ThumbnailProvider.hpp"

#include <QPainter>
#include <QPdfDocument>
#include <QQuickTextureFactory>
#include <atomic>

namespace {

constexpr int kCacheKiB = 192 * 1024; // ~1000 typical thumbnails
constexpr int kDefaultWidth = 200;
constexpr int kMaxEdge = 4096;

ThumbnailProvider* g_instance = nullptr;

// Renders on the provider's thread and reports back through a queued signal, so a response the
// engine has already discarded is never touched from the render thread.
class RenderTask : public QObject, public QRunnable {
    Q_OBJECT
  public:
    RenderTask(ThumbnailProvider* provider, pdfcore::DocId id, int page, QSize size,
               std::shared_ptr<std::atomic<bool>> cancelled)
        : m_provider(provider), m_id(id), m_page(page), m_size(size),
          m_cancelled(std::move(cancelled)) {}

    void run() override {
        QImage image;
        if (!m_cancelled->load()) {
            image = m_provider->render(m_id, m_page, m_size);
        }
        emit done(image);
    }

  signals:
    void done(const QImage& image);

  private:
    ThumbnailProvider* m_provider;
    pdfcore::DocId m_id;
    int m_page;
    QSize m_size;
    std::shared_ptr<std::atomic<bool>> m_cancelled;
};

class ThumbnailResponse : public QQuickImageResponse {
  public:
    ThumbnailResponse(ThumbnailProvider* provider, QThreadPool* pool, pdfcore::DocId id, int page,
                      QSize size) {
        auto* task = new RenderTask(provider, id, page, size, m_cancelled);
        connect(task, &RenderTask::done, this, [this](const QImage& image) {
            m_image = image;
            emit finished();
        });
        pool->start(task);
    }

    QQuickTextureFactory* textureFactory() const override {
        return QQuickTextureFactory::textureFactoryForImage(m_image);
    }

    QString errorString() const override {
        return m_image.isNull() ? QStringLiteral("Page could not be rendered") : QString();
    }

    void cancel() override { m_cancelled->store(true); }

  private:
    QImage m_image;
    std::shared_ptr<std::atomic<bool>> m_cancelled = std::make_shared<std::atomic<bool>>(false);
};

} // namespace

ThumbnailProvider::ThumbnailProvider() {
    g_instance = this;
    m_cache.setMaxCost(kCacheKiB);
    m_pool.setMaxThreadCount(1);
    m_pool.setExpiryTimeout(-1); // documents are bound to this one thread
}

ThumbnailProvider::~ThumbnailProvider() {
    m_pool.clear();
    m_pool.waitForDone();
    g_instance = nullptr;
}

ThumbnailProvider* ThumbnailProvider::instance() {
    return g_instance;
}

void ThumbnailProvider::registerDocument(pdfcore::DocId id, const QString& path,
                                         const QString& password) {
    QMutexLocker lock(&m_mutex);
    auto source = std::make_shared<Source>();
    source->path = path;
    source->password = password;
    m_sources.insert(id, std::move(source));
}

void ThumbnailProvider::unregisterDocument(pdfcore::DocId id) {
    std::shared_ptr<Source> source;
    {
        QMutexLocker lock(&m_mutex);
        source = m_sources.take(id);
    }
    if (source && source->document) {
        // Close on the render thread that owns it.
        m_pool.start([source] { source->document.reset(); });
    }
}

QQuickImageResponse* ThumbnailProvider::requestImageResponse(const QString& id,
                                                             const QSize& requestedSize) {
    const auto parts = id.split(u'/');
    const auto docId = parts.value(0).toUInt();
    const int page = parts.value(1).toInt();
    return new ThumbnailResponse(this, &m_pool, docId, page, requestedSize);
}

QImage ThumbnailProvider::render(pdfcore::DocId id, int page, const QSize& requestedSize) {
    const QSize want = requestedSize.isValid() && !requestedSize.isEmpty()
                           ? requestedSize.boundedTo(QSize(kMaxEdge, kMaxEdge))
                           : QSize(kDefaultWidth, 0);
    const QString key =
        QStringLiteral("%1/%2/%3x%4").arg(id).arg(page).arg(want.width()).arg(want.height());
    std::shared_ptr<Source> source;
    {
        QMutexLocker lock(&m_mutex);
        if (auto* cached = m_cache.object(key)) {
            return *cached;
        }
        source = m_sources.value(id);
    }
    if (!source) {
        return {};
    }
    if (!source->document) {
        source->document = std::make_unique<QPdfDocument>();
        source->document->setPassword(source->password);
        if (source->document->load(source->path) != QPdfDocument::Error::None) {
            source->document.reset();
            return {};
        }
    }
    auto* doc = source->document.get();
    if (page < 0 || page >= doc->pageCount()) {
        return {};
    }
    const QSizeF points = doc->pagePointSize(page);
    if (points.isEmpty()) {
        return {};
    }
    // Fit inside the requested box, keeping the page's aspect ratio.
    QSizeF target = points;
    if (want.width() > 0 && want.height() > 0) {
        target.scale(QSizeF(want), Qt::KeepAspectRatio);
    } else if (want.width() > 0) {
        target = QSizeF(want.width(), want.width() * points.height() / points.width());
    } else {
        target = QSizeF(want.height() * points.width() / points.height(), want.height());
    }
    const QSize pixels = target.toSize().expandedTo(QSize(1, 1));
    const QImage rendered = doc->render(page, pixels);
    if (rendered.isNull()) {
        return {};
    }
    // Pages are transparent where nothing is drawn; paper is white in both themes.
    QImage image(rendered.size(), QImage::Format_RGB32);
    image.fill(Qt::white);
    {
        QPainter painter(&image);
        painter.drawImage(0, 0, rendered);
    }
    {
        QMutexLocker lock(&m_mutex);
        m_cache.insert(key, new QImage(image), std::max<qsizetype>(1, image.sizeInBytes() / 1024));
    }
    return image;
}

#include "ThumbnailProvider.moc"
