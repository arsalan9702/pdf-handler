import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import PdfHandler

ToolLayout {
    id: page

    function addFiles(urls) { docs.addUrls(urls) }

    DocumentList { id: docs }

    title: qsTr("Unlock PDF")
    subtitle: qsTr("Remove the password and restrictions from files you're allowed to open.")
    files: DocumentPanel { documents: docs }

    actionText: docs.count > 1 ? qsTr("Unlock %n files", "", docs.count) : qsTr("Unlock")
    actionIcon: "lock-open"
    actionEnabled: docs.allReady
    actionHint: docs.count > 0 && !docs.allReady ? qsTr("Enter the password for each locked file on the left.") : ""
    onAction: {
        if (docs.count === 1) {
            saveDialog.currentFile = Tools.suggestedFile(docs, 0, "-unlocked")
            saveDialog.open()
        } else {
            folderDialog.currentFolder = Tools.folderOf(docs, 0)
            folderDialog.open()
        }
    }

    SectionTitle { text: qsTr("How it works") }
    HelpText {
        Layout.fillWidth: true
        text: qsTr("Files that need a password ask for it when you add them. The unlocked copy opens without a password and has no printing or copying restrictions.")
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: Tools.unlock(docs, selectedFile)
    }
    FolderDialog { id: folderDialog; onAccepted: Tools.unlock(docs, selectedFolder) }
}
