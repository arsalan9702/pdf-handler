import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import PdfHandler

ToolLayout {
    id: page

    function addFiles(urls) { docs.addUrls(urls) }

    DocumentList { id: docs }
    property int preset: Tools.Balanced

    title: qsTr("Compress PDF")
    subtitle: qsTr("Make files smaller. The original is never changed unless you choose to overwrite it.")
    files: DocumentPanel { documents: docs }

    actionText: docs.count > 1 ? qsTr("Compress %n files", "", docs.count) : qsTr("Compress")
    actionIcon: "shrink"
    actionEnabled: docs.allReady
    actionHint: docs.count > 1 ? qsTr("Files are saved to a folder you choose, with \"-compressed\" added to their names.") : ""
    onAction: {
        if (docs.count === 1) {
            saveDialog.currentFile = Tools.suggestedFile(docs, 0, "-compressed")
            saveDialog.open()
        } else {
            folderDialog.currentFolder = Tools.folderOf(docs, 0)
            folderDialog.open()
        }
    }

    SectionTitle { text: qsTr("Compression") }
    ButtonGroup { id: presets }
    OptionCard {
        Layout.fillWidth: true
        ButtonGroup.group: presets
        text: qsTr("Lossless")
        description: qsTr("Identical quality. Optimizes the file structure; best for text documents.")
        checked: page.preset === Tools.Lossless
        onClicked: page.preset = Tools.Lossless
    }
    OptionCard {
        Layout.fillWidth: true
        ButtonGroup.group: presets
        text: qsTr("Balanced")
        description: qsTr("Re-encodes large photos at good quality. Usually much smaller.")
        checked: page.preset === Tools.Balanced
        onClicked: page.preset = Tools.Balanced
    }
    OptionCard {
        Layout.fillWidth: true
        ButtonGroup.group: presets
        text: qsTr("Smallest")
        description: qsTr("Stronger image compression for email and uploads. Photos lose some detail.")
        checked: page.preset === Tools.Small
        onClicked: page.preset = Tools.Small
    }
    HelpText {
        Layout.fillWidth: true
        text: qsTr("Images that can't be re-encoded safely (CMYK, masks, scans in special formats) are left untouched, and an image is only replaced when the result is smaller.")
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: Tools.compress(docs, page.preset, selectedFile)
    }
    FolderDialog {
        id: folderDialog
        onAccepted: Tools.compress(docs, page.preset, selectedFolder)
    }
}
