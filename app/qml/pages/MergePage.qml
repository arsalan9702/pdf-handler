import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import PdfHandler

ToolLayout {
    id: page

    function addFiles(urls) { docs.addUrls(urls) }

    DocumentList { id: docs }

    title: qsTr("Merge PDFs")
    subtitle: qsTr("Combine files in the order shown. Pick page ranges per file if you only need some pages.")
    files: DocumentPanel { documents: docs; showRange: true; reorderable: true }

    actionText: qsTr("Merge %n file(s)", "", docs.count)
    actionIcon: "files"
    actionEnabled: docs.count >= 2 && docs.allReady
    actionHint: docs.count < 2 ? qsTr("Add at least two files") : ""
    onAction: {
        saveDialog.currentFile = Tools.suggestedFile(docs, 0, "-merged")
        saveDialog.open()
    }

    SectionTitle { text: qsTr("Options") }
    AppCheckBox {
        id: bookmarks
        Layout.fillWidth: true
        text: qsTr("Add a bookmark for each file")
    }
    HelpText {
        Layout.fillWidth: true
        text: qsTr("Bookmarks let readers jump to where each original file starts.")
    }
    HelpText {
        Layout.fillWidth: true
        text: qsTr("Need to reorder or rotate individual pages? Use Organize pages instead.")
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: Tools.merge(docs, selectedFile, bookmarks.checked)
    }
}
