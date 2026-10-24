import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import PdfHandler

ToolLayout {
    id: page

    function addFiles(urls) { docs.addUrls(urls.slice(0, 1)) }

    DocumentList { id: docs; maxDocuments: 1 }
    readonly property var dpis: [72, 150, 300, 600]
    readonly property string rangeError: docs.allReady ? docs.rangeError(0, range.text) : ""

    title: qsTr("PDF to images")
    subtitle: qsTr("Save pages as PNG or JPEG pictures.")
    files: DocumentPanel { documents: docs }

    actionText: qsTr("Export images")
    actionIcon: "image"
    actionEnabled: docs.allReady && rangeError.length === 0
    onAction: {
        folderDialog.currentFolder = Tools.folderOf(docs, 0)
        folderDialog.open()
    }

    SectionTitle { text: qsTr("Pages") }
    AppTextField {
        id: range
        Layout.fillWidth: true
        placeholderText: qsTr("All pages — or e.g. 1-3, 7, 10-")
        errorText: page.rangeError
    }
    HelpText { Layout.fillWidth: true; visible: page.rangeError.length > 0; error: true; text: page.rangeError }

    SectionTitle { text: qsTr("Format"); Layout.topMargin: Theme.spacingS }
    SegmentedControl { id: format; Layout.fillWidth: true; options: ["PNG", "JPEG"] }
    HelpText {
        Layout.fillWidth: true
        text: format.currentIndex === 0 ? qsTr("Lossless and sharp; best for text and diagrams.")
                                        : qsTr("Much smaller files; best for photos.")
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: format.currentIndex === 1
        spacing: Theme.spacingXs
        RowLayout {
            FieldLabel { text: qsTr("Quality"); Layout.fillWidth: true }
            Label { text: Math.round(quality.value); color: Theme.textSecondary; font.pixelSize: Theme.fontSizeS }
        }
        AppSlider { id: quality; Layout.fillWidth: true; from: 40; to: 100; value: 90; stepSize: 1 }
    }

    SectionTitle { text: qsTr("Resolution"); Layout.topMargin: Theme.spacingS }
    SegmentedControl {
        id: dpi
        Layout.fillWidth: true
        options: ["72", "150", "300", "600"]
        currentIndex: 1
    }
    HelpText {
        Layout.fillWidth: true
        text: qsTr("Dots per inch. 150 suits screens, 300 suits printing.")
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose a folder for the images")
        onAccepted: Tools.pdfToImages(docs, 0, range.text, page.dpis[dpi.currentIndex],
                                      format.currentIndex === 0 ? "png" : "jpeg",
                                      Math.round(quality.value), selectedFolder)
    }
}
