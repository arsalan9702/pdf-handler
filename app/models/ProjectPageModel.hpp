#pragma once

#include "commands/PageCommands.hpp"
#include "core/Types.hpp"
#include "models/DocumentListModel.hpp"

#include <QAbstractListModel>
#include <QPointer>
#include <QSet>
#include <QUndoStack>
#include <QtQml/qqmlregistration.h>

// The organizer's ordered page list (the Project). Pages of each document are appended when
// it finishes loading. All edits go through the undo stack; nothing is written until export.
class ProjectPageModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(PageProject)
    Q_PROPERTY(
        DocumentListModel* documents READ documents WRITE setDocuments NOTIFY documentsChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoStateChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoStateChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY undoStateChanged)
    // Bumped on every change so QML bindings over pageAt() re-evaluate.
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

  public:
    enum Roles {
        DocIdRole = Qt::UserRole + 1,
        PageIndexRole,
        RotationRole,
        PageNumberRole,
        DocNameRole,
        DocIndexRole,
        SelectedRole,
        ThumbnailRole,
    };

    explicit ProjectPageModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    DocumentListModel* documents() const { return m_documents; }
    void setDocuments(DocumentListModel* documents);

    // Selection (not part of undo history).
    Q_INVOKABLE void select(int row, bool toggle, bool extend);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE bool isSelected(int row) const;

    // Edits act on page `row`, or on the selection when `row` is -1.
    Q_INVOKABLE void rotate(int row, int degrees);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void duplicate(int row);
    // Moves page `row` -- or the whole selection when `row` is part of it, as when dragging a
    // selected page -- so it starts at `target` (index in the current list).
    Q_INVOKABLE void moveTo(int row, int target);
    Q_INVOKABLE void reverse();

    // Roles of one row as a map, for views that show a single page (the preview).
    Q_INVOKABLE QVariantMap pageAt(int row) const;

    Q_INVOKABLE void undo() { m_undo.undo(); }
    Q_INVOKABLE void redo() { m_undo.redo(); }

    [[nodiscard]] int selectedCount() const;
    [[nodiscard]] bool canUndo() const { return m_undo.canUndo(); }
    [[nodiscard]] bool canRedo() const { return m_undo.canRedo(); }
    [[nodiscard]] QString undoText() const { return m_undo.undoText(); }
    [[nodiscard]] QString redoText() const { return m_undo.redoText(); }
    [[nodiscard]] bool modified() const { return !m_undo.isClean(); }
    [[nodiscard]] int revision() const { return m_revision; }

    // The Project for export: sources in document order, pages in organizer order
    // (only the selected ones when `selectedOnly`).
    [[nodiscard]] pdfcore::Project project(bool selectedOnly = false) const;
    void markSaved() { m_undo.setClean(); }

    // Used by the undo commands.
    void applyPages(const std::vector<PageItem>& pages);

  signals:
    void documentsChanged();
    void countChanged();
    void selectionChanged();
    void undoStateChanged();
    void revisionChanged();

  private:
    void appendDocument(quint32 docId);
    void dropDocument(quint32 docId);
    [[nodiscard]] QSet<quint64> targetUids(int row) const;
    [[nodiscard]] QSet<quint64> dragUids(int row) const;
    void push(SnapshotCommand* command);
    [[nodiscard]] PageItem makeItem(const pdfcore::PageRef& ref);

    QPointer<DocumentListModel> m_documents;
    std::vector<PageItem> m_pages;
    QSet<quint64> m_selected;
    int m_anchor = -1;
    quint64 m_nextUid = 1;
    int m_revision = 0;
    QUndoStack m_undo;
};
