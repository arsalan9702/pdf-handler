#include "models/ImageListModel.hpp"

#include <QFileInfo>
#include <QImageReader>

ImageListModel::ImageListModel(QObject* parent) : QAbstractListModel(parent) {}

int ImageListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_paths.size());
}

QVariant ImageListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount()) {
        return {};
    }
    const QString& path = m_paths.at(index.row());
    switch (role) {
    case PathRole:
        return path;
    case Qt::DisplayRole:
    case NameRole:
        return QFileInfo(path).fileName();
    case UrlRole:
        return QUrl::fromLocalFile(path);
    case SizeRole:
        return QFileInfo(path).size();
    default:
        return {};
    }
}

QHash<int, QByteArray> ImageListModel::roleNames() const {
    return {{PathRole, "path"}, {NameRole, "name"}, {UrlRole, "url"}, {SizeRole, "size"}};
}

int ImageListModel::addUrls(const QList<QUrl>& urls) {
    int skipped = 0;
    for (const auto& url : urls) {
        const QString path = url.toLocalFile();
        QImageReader reader(path);
        // Qt PDF installs an image plugin that "reads" PDFs; those belong in the other tools.
        if (path.isEmpty() || !reader.canRead() || reader.format() == "pdf") {
            ++skipped;
            continue;
        }
        const int row = rowCount();
        beginInsertRows({}, row, row);
        m_paths.append(QFileInfo(path).absoluteFilePath());
        endInsertRows();
    }
    emit countChanged();
    return skipped;
}

void ImageListModel::remove(int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    beginRemoveRows({}, row, row);
    m_paths.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

void ImageListModel::move(int from, int to) {
    const int n = rowCount();
    if (from < 0 || from >= n || to < 0 || to >= n || from == to) {
        return;
    }
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_paths.move(from, to);
    endMoveRows();
}

void ImageListModel::clear() {
    beginResetModel();
    m_paths.clear();
    endResetModel();
    emit countChanged();
}

QString ImageListModel::path(int row) const {
    return m_paths.value(row);
}
