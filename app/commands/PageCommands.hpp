#pragma once

#include "core/Types.hpp"

#include <QUndoCommand>
#include <vector>

class ProjectPageModel;

// One page in the organizer. `uid` is stable across edits so selection survives undo/redo.
struct PageItem {
    quint64 uid = 0;
    pdfcore::PageRef ref;
};

// Every organizer edit stores the page list before and after, which makes undo exact and
// keeps each command trivial. A 2,000-page list is ~50 KB per step.
class SnapshotCommand : public QUndoCommand {
  public:
    SnapshotCommand(ProjectPageModel* model, std::vector<PageItem> before,
                    std::vector<PageItem> after, const QString& text);
    void undo() override;
    void redo() override;

  private:
    ProjectPageModel* m_model;
    std::vector<PageItem> m_before;
    std::vector<PageItem> m_after;
};

class MovePagesCommand : public SnapshotCommand {
  public:
    using SnapshotCommand::SnapshotCommand;
};

class RotatePagesCommand : public SnapshotCommand {
  public:
    using SnapshotCommand::SnapshotCommand;
};

class DeletePagesCommand : public SnapshotCommand {
  public:
    using SnapshotCommand::SnapshotCommand;
};

class InsertPagesCommand : public SnapshotCommand {
  public:
    using SnapshotCommand::SnapshotCommand;
};
