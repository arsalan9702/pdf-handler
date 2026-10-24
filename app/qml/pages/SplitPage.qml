import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import PdfHandler

ToolLayout {
    id: page

    function addFiles(urls) { docs.addUrls(urls.slice(0, 1)) }

    DocumentList { id: docs; maxDocuments: 1 }

    // Recomputed whenever an option changes; shows the files that will be created.
    readonly property var preview: docs.allReady
        ? Tools.splitPreview(docs, 0, mode.currentIndex, parseInt(everyN.text) || 1, ranges.text, nameTemplate.text)
        : ({})

    title: qsTr("Split PDF")
    subtitle: qsTr("Break one PDF into several files.")
    files: DocumentPanel { documents: docs }

    actionText: preview.names ? qsTr("Split into %n file(s)", "", preview.names.length) : qsTr("Split")
    actionIcon: "scissors"
    actionEnabled: docs.allReady && !!preview.names
    onAction: {
        folderDialog.currentFolder = Tools.folderOf(docs, 0)
        folderDialog.open()
    }

    SectionTitle { text: qsTr("Split") }
    SegmentedControl {
        id: mode
        Layout.fillWidth: true
        options: [qsTr("Every page"), qsTr("Every N pages"), qsTr("By ranges")]
    }

    ColumnLayout {
        visible: mode.currentIndex === 1
        Layout.fillWidth: true
        spacing: Theme.spacingXs
        FieldLabel { text: qsTr("Pages per file") }
        AppTextField {
            id: everyN
            Layout.fillWidth: true
            text: "2"
            validator: IntValidator { bottom: 1; top: 100000 }
            inputMethodHints: Qt.ImhDigitsOnly
        }
    }

    ColumnLayout {
        visible: mode.currentIndex === 2
        Layout.fillWidth: true
        spacing: Theme.spacingXs
        FieldLabel { text: qsTr("One range per line (each becomes a file)") }
        TextArea {
            id: ranges
            Layout.fillWidth: true
            Layout.preferredHeight: 104
            placeholderText: "1-3\n4-10\n11-"
            placeholderTextColor: Theme.textTertiary
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeM
            wrapMode: TextEdit.NoWrap
            background: Rectangle {
                radius: Theme.radiusControl
                color: Theme.inputBackground
                border.width: ranges.activeFocus ? 2 : 1
                border.color: page.preview.error && mode.currentIndex === 2 ? Theme.danger
                            : ranges.activeFocus ? Theme.accent : Theme.borderStrong
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingXs
        FieldLabel { text: qsTr("File names") }
        AppTextField {
            id: nameTemplate
            Layout.fillWidth: true
            text: "{name}-{n}"
        }
        HelpText {
            Layout.fillWidth: true
            text: qsTr("{name} original name, {n} part number, {pages} page range")
        }
    }

    HelpText {
        Layout.fillWidth: true
        visible: !!page.preview.error
        error: true
        text: page.preview.error || ""
    }

    ColumnLayout {
        visible: !!page.preview.names
        Layout.fillWidth: true
        spacing: Theme.spacingXs
        FieldLabel { text: qsTr("Preview") }
        Repeater {
            model: page.preview.names ? page.preview.names.slice(0, 6) : []
            delegate: RowLayout {
                required property string modelData
                Layout.fillWidth: true
                spacing: Theme.spacingS
                AppIcon { name: "file-text"; size: Theme.iconSizeSmall; color: Theme.textTertiary }
                Label {
                    Layout.fillWidth: true
                    text: modelData
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeS
                    elide: Text.ElideMiddle
                }
            }
        }
        HelpText {
            visible: page.preview.names && page.preview.names.length > 6
            text: page.preview.names ? qsTr("…and %n more", "", page.preview.names.length - 6) : ""
        }
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Choose where to save the parts")
        onAccepted: Tools.split(docs, 0, mode.currentIndex, parseInt(everyN.text) || 1, ranges.text,
                                nameTemplate.text, selectedFolder)
    }
}
