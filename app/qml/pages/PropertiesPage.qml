import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import PdfHandler

ToolLayout {
    id: page

    function addFiles(urls) { docs.addUrls(urls.slice(0, 1)) }

    DocumentList {
        id: docs
        maxDocuments: 1
        onStatusChanged: page.info = allReady ? Tools.readMetadata(docs, 0) : ({})
    }
    property var info: ({})

    title: qsTr("Document properties")
    subtitle: qsTr("Edit the title, author and other details shown by PDF viewers and search tools.")
    files: DocumentPanel { documents: docs }

    actionText: qsTr("Save as…")
    actionIcon: "download"
    actionEnabled: docs.allReady
    onAction: {
        saveDialog.currentFile = Tools.suggestedFile(docs, 0, "")
        saveDialog.open()
    }

    SectionTitle { text: qsTr("Details") }
    Repeater {
        id: fields
        model: [
            { key: "title", label: qsTr("Title") },
            { key: "author", label: qsTr("Author") },
            { key: "subject", label: qsTr("Subject") },
            { key: "keywords", label: qsTr("Keywords") },
            { key: "creator", label: qsTr("Created with") },
            { key: "producer", label: qsTr("PDF producer") }
        ]
        delegate: ColumnLayout {
            required property var modelData
            property alias value: field.text
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            FieldLabel { text: modelData.label }
            AppTextField {
                id: field
                Layout.fillWidth: true
                enabled: docs.allReady
                text: page.info[modelData.key] || ""
            }
        }
    }

    HelpText {
        Layout.fillWidth: true
        visible: !!page.info.pdfVersion
        text: qsTr("PDF version %1").arg(page.info.pdfVersion || "")
              + (page.info.modified ? "  ·  " + qsTr("last modified %1").arg(page.info.modified) : "")
    }
    HelpText {
        Layout.fillWidth: true
        visible: !!page.info.hasXmp
        text: qsTr("This file also contains XMP metadata, which some apps show instead. It is kept as is.")
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: {
            const values = {}
            for (let i = 0; i < fields.count; ++i)
                values[fields.model[i].key] = fields.itemAt(i).value
            Tools.writeMetadata(docs, 0, values, selectedFile)
        }
    }
}
