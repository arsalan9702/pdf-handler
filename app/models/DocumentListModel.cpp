#include "models/DocumentListModel.hpp"

#include "core/Document.hpp"
#include "core/PageRange.hpp"
#include "services/ThumbnailProvider.hpp"

#include <QFileInfo>
#include <QPointer>
#include <QThreadPool>
#include <atomic>

namespace {

std::atomic<pdfcore::DocId> g_nextId{1};

std::filesystem::path toPath(const QString& path) {
    return {path.toStdU16String()};
}

} // namespace

DocumentListModel::DocumentListModel(QObject* parent) : QAbstractListModel(parent) {}

DocumentListModel::~DocumentListModel() {
    if (auto* thumbs = ThumbnailProvider::instance()) {
        for (const auto& e : m_entries) {
            thumbs->unregisterDocument(e.id);
        }
    }
}

int DocumentListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant DocumentListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount()) {
        return {};
    }
    const auto& e = m_entries[static_cast<std::size_t>(index.row())];
    const QFileInfo info(e.path);
    switch (role) {
    case DocIdRole:
        return e.id;
    case PathRole:
        return e.path;
    case Qt::DisplayRole:
    case NameRole:
        return info.fileName();
    case FolderRole:
        return info.absolutePath();
    case PageCountRole:
        return e.pageCount;
    case EncryptedRole:
        return e.encrypted;
    case StatusRole:
        return e.status;
    case ErrorRole:
        return e.error;
    case WarningsRole:
        return e.warnings;
    case SizeRole:
        return e.size;
    case RangeRole:
        return e.range;
    default:
        return {};
    }
}

QHash<int, QByteArray> DocumentListModel::roleNames() const {
    return {{DocIdRole, "docId"},   {PathRole, "path"},           {NameRole, "name"},
            {FolderRole, "folder"}, {PageCountRole, "pageCount"}, {EncryptedRole, "encrypted"},
            {StatusRole, "status"}, {ErrorRole, "error"},         {WarningsRole, "warnings"},
            {SizeRole, "size"},     {RangeRole, "range"}};
}

void DocumentListModel::addUrls(const QList<QUrl>& urls) {
    for (const auto& url : urls) {
        if (m_maxDocuments > 0 && rowCount() >= m_maxDocuments) {
            if (m_maxDocuments == 1 && rowCount() == 1) {
                remove(0); // single-document tools: the new file replaces the old one
            } else {
                break;
            }
        }
        const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        Entry e;
        e.id = g_nextId++;
        e.path = QFileInfo(path).absoluteFilePath();
        e.size = QFileInfo(path).size();
        const int row = rowCount();
        beginInsertRows({}, row, row);
        m_entries.push_back(e);
        endInsertRows();
        inspect(e.id, e.path, {});
    }
    emit countChanged();
    emit statusChanged();
}

void DocumentListModel::inspect(pdfcore::DocId id, const QString& path, const QString& password) {
    QPointer<DocumentListModel> self(this);
    QThreadPool::globalInstance()->start([self, id, path, password] {
        Entry result;
        try {
            auto doc = pdfcore::openDocument(toPath(path), password.toStdString());
            result.pageCount = doc.pageCount;
            result.encrypted = doc.encrypted;
            result.status = Ready;
            for (const auto& w : doc.warnings) {
                result.warnings << QString::fromUtf8(w);
            }
        } catch (const pdfcore::PdfError& e) {
            switch (e.kind()) {
            case pdfcore::ErrorKind::PasswordRequired:
                result.status = NeedsPassword;
                break;
            case pdfcore::ErrorKind::WrongPassword:
                result.status = WrongPassword;
                break;
            case pdfcore::ErrorKind::NotAPdf:
                result.status = Failed;
                result.error = tr("Not a PDF file");
                break;
            default:
                result.status = Failed;
                result.error = QString::fromUtf8(e.what());
            }
        } catch (const std::exception& e) {
            result.status = Failed;
            result.error = QString::fromUtf8(e.what());
        }
        QMetaObject::invokeMethod(
            self,
            [self, id, password, result] {
                if (!self) {
                    return;
                }
                const int row = self->rowOf(id);
                if (row < 0) {
                    return; // removed while loading
                }
                auto& e = self->m_entries[static_cast<std::size_t>(row)];
                e.pageCount = result.pageCount;
                e.encrypted = result.encrypted;
                e.status = result.status;
                e.error = result.error;
                e.warnings = result.warnings;
                if (e.status == Ready) {
                    e.password = password;
                    if (auto* thumbs = ThumbnailProvider::instance()) {
                        thumbs->registerDocument(e.id, e.path, password);
                    }
                }
                self->changed(row);
                if (e.status == Ready) {
                    emit self->documentReady(e.id);
                }
            },
            Qt::QueuedConnection);
    });
}

void DocumentListModel::unlock(int row, const QString& password) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    auto& e = m_entries[static_cast<std::size_t>(row)];
    e.status = Loading;
    changed(row);
    inspect(e.id, e.path, password);
}

void DocumentListModel::remove(int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    const auto id = m_entries[static_cast<std::size_t>(row)].id;
    beginRemoveRows({}, row, row);
    m_entries.erase(m_entries.begin() + row);
    endRemoveRows();
    if (auto* thumbs = ThumbnailProvider::instance()) {
        thumbs->unregisterDocument(id);
    }
    emit documentRemoved(id);
    emit countChanged();
    emit statusChanged();
}

void DocumentListModel::move(int from, int to) {
    const int n = rowCount();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) {
        return;
    }
    // beginMoveRows wants the destination as "insert before" in the pre-move list.
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    auto entry = std::move(m_entries[static_cast<std::size_t>(from)]);
    m_entries.erase(m_entries.begin() + from);
    m_entries.insert(m_entries.begin() + to, std::move(entry));
    endMoveRows();
}

void DocumentListModel::clear() {
    while (rowCount() > 0) {
        remove(rowCount() - 1);
    }
}

void DocumentListModel::setRange(int row, const QString& range) {
    if (row >= 0 && row < rowCount()) {
        m_entries[static_cast<std::size_t>(row)].range = range;
        const auto idx = index(row);
        emit dataChanged(idx, idx, {RangeRole});
    }
}

QString DocumentListModel::rangeError(int row, const QString& range) const {
    if (row < 0 || row >= rowCount() || range.trimmed().isEmpty()) {
        return {};
    }
    const auto& e = m_entries[static_cast<std::size_t>(row)];
    if (e.status != Ready) {
        return {};
    }
    auto parsed = pdfcore::parsePageRange(range.toStdString(), e.pageCount);
    return parsed ? QString() : QString::fromUtf8(parsed.error().message);
}

QString DocumentListModel::path(int row) const {
    return row >= 0 && row < rowCount() ? m_entries[static_cast<std::size_t>(row)].path : QString();
}

int DocumentListModel::pageCount(int row) const {
    return row >= 0 && row < rowCount() ? m_entries[static_cast<std::size_t>(row)].pageCount : 0;
}

int DocumentListModel::readyCount() const {
    return static_cast<int>(
        std::ranges::count_if(m_entries, [](const Entry& e) { return e.status == Ready; }));
}

bool DocumentListModel::allReady() const {
    return !m_entries.empty() && readyCount() == rowCount();
}

void DocumentListModel::setMaxDocuments(int max) {
    if (m_maxDocuments != max) {
        m_maxDocuments = max;
        emit maxDocumentsChanged();
    }
}

const DocumentListModel::Entry* DocumentListModel::findById(pdfcore::DocId id) const {
    const int row = rowOf(id);
    return row < 0 ? nullptr : &m_entries[static_cast<std::size_t>(row)];
}

pdfcore::SourceDoc DocumentListModel::source(int row) const {
    const auto& e = m_entries.at(static_cast<std::size_t>(row));
    return {e.id, toPath(e.path), e.password.toStdString()};
}

int DocumentListModel::rowOf(pdfcore::DocId id) const {
    for (std::size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void DocumentListModel::changed(int row) {
    const auto idx = index(row);
    emit dataChanged(idx, idx);
    emit statusChanged();
}
