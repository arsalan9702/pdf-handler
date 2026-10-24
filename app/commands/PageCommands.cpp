#include "commands/PageCommands.hpp"

#include "models/ProjectPageModel.hpp"

SnapshotCommand::SnapshotCommand(ProjectPageModel* model, std::vector<PageItem> before,
                                 std::vector<PageItem> after, const QString& text)
    : m_model(model), m_before(std::move(before)), m_after(std::move(after)) {
    setText(text);
}

void SnapshotCommand::undo() {
    m_model->applyPages(m_before);
}

void SnapshotCommand::redo() {
    m_model->applyPages(m_after);
}
