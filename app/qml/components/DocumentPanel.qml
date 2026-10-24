import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import PdfHandler

// The list of files a tool works on, with an empty state that accepts drops.
Item {
    id: root

    required property DocumentList documents
    property bool showRange: false
    property bool reorderable: false
    property string emptyTitle: documents.maxDocuments === 1 ? qsTr("Drop a PDF file here")
                                                             : qsTr("Drop PDF files here")

    DropZone {
        anchors.fill: parent
        visible: root.documents.count === 0
        title: root.emptyTitle
        multiple: root.documents.maxDocuments !== 1
        onFilesChosen: (urls) => root.documents.addUrls(urls)
    }

    ColumnLayout {
        anchors.fill: parent
        visible: root.documents.count > 0
        spacing: Theme.spacingS

        RowLayout {
            Label {
                Layout.fillWidth: true
                text: root.documents.maxDocuments === 1 ? qsTr("File")
                                                        : qsTr("%n file(s)", "", root.documents.count)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeS
                font.weight: Theme.weightMedium
            }
            AppButton {
                compact: true
                kind: "ghost"
                iconName: root.documents.maxDocuments === 1 ? "folder-open" : "plus"
                text: root.documents.maxDocuments === 1 ? qsTr("Choose another") : qsTr("Add files")
                onClicked: picker.open()
            }
            AppButton {
                compact: true
                kind: "ghost"
                visible: root.documents.count > 1
                text: qsTr("Clear")
                onClicked: root.documents.clear()
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacingS
            model: root.documents
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            moveDisplaced: Transition { NumberAnimation { properties: "y"; duration: Theme.durationNormal; easing.type: Easing.OutCubic } }
            move: Transition { NumberAnimation { properties: "y"; duration: Theme.durationNormal; easing.type: Easing.OutCubic } }
            delegate: DocumentRow {
                width: ListView.view.width - (list.ScrollBar.vertical.visible ? Theme.spacingS + Theme.spacingXs : 0)
                documents: root.documents
                showRange: root.showRange
                reorderable: root.reorderable && root.documents.count > 1
            }

            DropArea {
                anchors.fill: parent
                onDropped: (event) => { if (event.hasUrls) root.documents.addUrls(event.urls) }
            }
        }
    }

    FileDialog {
        id: picker
        fileMode: root.documents.maxDocuments === 1 ? FileDialog.OpenFile : FileDialog.OpenFiles
        nameFilters: [qsTr("PDF files (*.pdf)"), qsTr("All files (*)")]
        onAccepted: root.documents.addUrls(fileMode === FileDialog.OpenFile ? [selectedFile] : selectedFiles)
    }
}
