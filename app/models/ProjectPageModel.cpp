#include "models/ProjectPageModel.hpp"

#include "models/DocumentListModel.hpp"

#include <QFileInfo>
#include <algorithm>

ProjectPageModel::ProjectPageModel(QObject* parent) : QAbstractListModel(parent) {
    connect(&m_undo, &QUndoStack::indexChanged, this, &ProjectPageModel::undoStateChanged);
    connect(&m_undo, &QUndoStack::cleanChanged, this, &ProjectPageModel::undoStateChanged);
}

int ProjectPageModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_pages.size());
}

QVariant ProjectPageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= rowCount()) {
        return {};
    }
    const auto& item = m_pages[static_cast<std::size_t>(index.row())];
    const auto& ref = item.ref;
    switch (role) {
    case DocIdRole:
        return ref.sourceDocId;
    case PageIndexRole:
        return ref.pageIndex;
    case RotationRole:
        return ref.rotation;
    case PageNumberRole:
        return ref.pageIndex + 1;
    case DocNameRole:
        if (m_documents) {
            if (const auto* e = m_documents->findById(ref.sourceDocId)) {
                return QFileInfo(e->path).completeBaseName();
            }
        }
        return {};
    case DocIndexRole:
        if (m_documents) {
            const auto& entries = m_documents->entries();
            for (std::size_t i = 0; i < entries.size(); ++i) {
                if (entries[i].id == ref.sourceDocId) {
                    return static_cast<int>(i);
                }
            }
        }
        return 0;
    case SelectedRole:
        return m_selected.contains(item.uid);
    case ThumbnailRole:
        return QStringLiteral("image://thumb/%1/%2").arg(ref.sourceDocId).arg(ref.pageIndex);
    default:
        return {};
    }
}

QHash<int, QByteArray> ProjectPageModel::roleNames() const {
    return {{DocIdRole, "docId"},           {PageIndexRole, "pageIndex"},
            {RotationRole, "pageRotation"}, {PageNumberRole, "pageNumber"},
            {DocNameRole, "docName"},       {DocIndexRole, "docIndex"},
            {SelectedRole, "selected"},     {ThumbnailRole, "thumbnail"}};
}

void ProjectPageModel::setDocuments(DocumentListModel* documents) {
    if (m_documents == documents) {
        return;
    }
    if (m_documents) {
        disconnect(m_documents, nullptr, this, nullptr);
    }
    m_documents = documents;
    if (m_documents) {
        connect(m_documents, &DocumentListModel::documentReady, this,
                &ProjectPageModel::appendDocument);
        connect(m_documents, &DocumentListModel::documentRemoved, this,
                &ProjectPageModel::dropDocument);
        connect(m_documents, &QAbstractItemModel::rowsMoved, this,
                [this] { emit dataChanged(index(0), index(rowCount() - 1), {DocIndexRole}); });
    }
    emit documentsChanged();
}

PageItem ProjectPageModel::makeItem(const pdfcore::PageRef& ref) {
    return {m_nextUid++, ref};
}

void ProjectPageModel::appendDocument(quint32 docId) {
    const auto* entry = m_documents ? m_documents->findById(docId) : nullptr;
    if (entry == nullptr) {
        return;
    }
    // Files finish loading in any order; keep their pages in file-list order by inserting
    // before the first page of any document listed after this one.
    const auto& entries = m_documents->entries();
    auto rowOf = [&entries](pdfcore::DocId id) {
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (entries[i].id == id) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };
    const int row = rowOf(docId);
    auto at = std::ranges::find_if(
        m_pages, [&](const PageItem& p) { return rowOf(p.ref.sourceDocId) > row; });
    std::vector<PageItem> inserted;
    for (int i = 0; i < entry->pageCount; ++i) {
        inserted.push_back(makeItem({docId, i, 0}));
    }
    auto after = m_pages;
    after.insert(after.begin() + (at - m_pages.begin()), inserted.begin(), inserted.end());

    // Files added before any edit are the starting point, not an undoable step.
    if (m_undo.count() == 0) {
        applyPages(after);
    } else {
        push(new InsertPagesCommand(this, m_pages, std::move(after),
                                    tr("Add %1").arg(QFileInfo(entry->path).fileName())));
    }
}

void ProjectPageModel::dropDocument(quint32 docId) {
    // Removing a source file is not undoable (its pages could not be restored), so history
    // that references it is discarded too.
    std::vector<PageItem> kept;
    std::ranges::copy_if(m_pages, std::back_inserter(kept),
                         [docId](const PageItem& p) { return p.ref.sourceDocId != docId; });
    applyPages(kept);
    m_undo.clear();
}

void ProjectPageModel::applyPages(const std::vector<PageItem>& pages) {
    const bool sameOrder =
        pages.size() == m_pages.size() &&
        std::ranges::equal(pages, m_pages,
                           [](const PageItem& a, const PageItem& b) { return a.uid == b.uid; });
    if (sameOrder) {
        m_pages = pages; // rotation only: keep delegates, let them animate
        emit dataChanged(index(0), index(rowCount() - 1), {RotationRole});
    } else {
        beginResetModel();
        m_pages = pages;
        endResetModel();
        emit countChanged();
    }
    ++m_revision;
    emit revisionChanged();
    // Forget selected pages that no longer exist.
    QSet<quint64> alive;
    for (const auto& p : m_pages) {
        if (m_selected.contains(p.uid)) {
            alive.insert(p.uid);
        }
    }
    if (alive != m_selected) {
        m_selected = alive;
        emit selectionChanged();
    }
}

void ProjectPageModel::select(int row, bool toggle, bool extend) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    const quint64 uid = m_pages[static_cast<std::size_t>(row)].uid;
    if (extend && m_anchor >= 0 && m_anchor < rowCount()) {
        if (!toggle) {
            m_selected.clear();
        }
        for (int i = std::min(row, m_anchor); i <= std::max(row, m_anchor); ++i) {
            m_selected.insert(m_pages[static_cast<std::size_t>(i)].uid);
        }
    } else if (toggle) {
        if (!m_selected.remove(uid)) {
            m_selected.insert(uid);
        }
        m_anchor = row;
    } else {
        m_selected = {uid};
        m_anchor = row;
    }
    emit dataChanged(index(0), index(rowCount() - 1), {SelectedRole});
    emit selectionChanged();
}

void ProjectPageModel::selectAll() {
    for (const auto& p : m_pages) {
        m_selected.insert(p.uid);
    }
    emit dataChanged(index(0), index(rowCount() - 1), {SelectedRole});
    emit selectionChanged();
}

void ProjectPageModel::clearSelection() {
    if (m_selected.isEmpty()) {
        return;
    }
    m_selected.clear();
    m_anchor = -1;
    emit dataChanged(index(0), index(rowCount() - 1), {SelectedRole});
    emit selectionChanged();
}

bool ProjectPageModel::isSelected(int row) const {
    return row >= 0 && row < rowCount() &&
           m_selected.contains(m_pages[static_cast<std::size_t>(row)].uid);
}

int ProjectPageModel::selectedCount() const {
    return static_cast<int>(m_selected.size());
}

QSet<quint64> ProjectPageModel::targetUids(int row) const {
    if (row < 0) {
        return m_selected;
    }
    if (row < rowCount()) {
        return {m_pages[static_cast<std::size_t>(row)].uid};
    }
    return {};
}

QSet<quint64> ProjectPageModel::dragUids(int row) const {
    return isSelected(row) ? m_selected : targetUids(row);
}

void ProjectPageModel::push(SnapshotCommand* command) {
    m_undo.push(command); // QUndoStack calls redo(), which applies the new list
}

void ProjectPageModel::rotate(int row, int degrees) {
    const auto targets = targetUids(row);
    if (targets.isEmpty()) {
        return;
    }
    auto after = m_pages;
    for (auto& p : after) {
        if (targets.contains(p.uid)) {
            p.ref.rotation = ((p.ref.rotation + degrees) % 360 + 360) % 360;
        }
    }
    push(new RotatePagesCommand(this, m_pages, std::move(after),
                                targets.size() == 1 ? tr("Rotate page")
                                                    : tr("Rotate %n pages", "", targets.size())));
}

void ProjectPageModel::remove(int row) {
    const auto targets = targetUids(row);
    if (targets.isEmpty()) {
        return;
    }
    std::vector<PageItem> after;
    std::ranges::copy_if(m_pages, std::back_inserter(after),
                         [&](const PageItem& p) { return !targets.contains(p.uid); });
    push(new DeletePagesCommand(this, m_pages, std::move(after),
                                targets.size() == 1 ? tr("Delete page")
                                                    : tr("Delete %n pages", "", targets.size())));
}

void ProjectPageModel::duplicate(int row) {
    const auto targets = targetUids(row);
    if (targets.isEmpty()) {
        return;
    }
    std::vector<PageItem> after;
    for (const auto& p : m_pages) {
        after.push_back(p);
        if (targets.contains(p.uid)) {
            after.push_back(makeItem(p.ref));
        }
    }
    push(new InsertPagesCommand(
        this, m_pages, std::move(after),
        targets.size() == 1 ? tr("Duplicate page") : tr("Duplicate %n pages", "", targets.size())));
}

void ProjectPageModel::moveTo(int row, int target) {
    const auto targets = dragUids(row);
    if (targets.isEmpty()) {
        return;
    }
    std::vector<PageItem> moving;
    std::vector<PageItem> rest;
    int insertAt = 0;
    for (int i = 0; i < rowCount(); ++i) {
        const auto& p = m_pages[static_cast<std::size_t>(i)];
        if (targets.contains(p.uid)) {
            moving.push_back(p);
        } else {
            if (i < target) {
                ++insertAt;
            }
            rest.push_back(p);
        }
    }
    auto after = rest;
    after.insert(after.begin() + std::min<std::ptrdiff_t>(insertAt, std::ssize(after)),
                 moving.begin(), moving.end());
    if (std::ranges::equal(after, m_pages,
                           [](const PageItem& a, const PageItem& b) { return a.uid == b.uid; })) {
        return;
    }
    push(new MovePagesCommand(this, m_pages, std::move(after),
                              moving.size() == 1 ? tr("Move page")
                                                 : tr("Move %n pages", "", moving.size())));
}

void ProjectPageModel::reverse() {
    if (m_pages.size() < 2) {
        return;
    }
    auto after = m_pages;
    std::ranges::reverse(after);
    push(new MovePagesCommand(this, m_pages, std::move(after), tr("Reverse order")));
}

QVariantMap ProjectPageModel::pageAt(int row) const {
    if (row < 0 || row >= rowCount()) {
        return {};
    }
    QVariantMap map;
    const auto roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        map.insert(QString::fromLatin1(it.value()), data(index(row), it.key()));
    }
    return map;
}

pdfcore::Project ProjectPageModel::project(bool selectedOnly) const {
    pdfcore::Project p;
    if (m_documents) {
        for (int i = 0; i < m_documents->rowCount(); ++i) {
            p.sources.push_back(m_documents->source(i));
        }
    }
    for (const auto& item : m_pages) {
        if (!selectedOnly || m_selected.contains(item.uid)) {
            p.pages.push_back(item.ref);
        }
    }
    return p;
}
