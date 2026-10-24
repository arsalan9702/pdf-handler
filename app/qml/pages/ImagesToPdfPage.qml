import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import PdfHandler

ToolLayout {
    id: page

    function addFiles(urls) {
        const skipped = images.addUrls(urls)
        if (skipped > 0)
            toastRequested("warning", qsTr("%n file(s) skipped", "", skipped), qsTr("Only image files can be added here."))
    }
    signal toastRequested(string kind, string title, string message)

    ImageList { id: images }

    title: qsTr("Images to PDF")
    subtitle: qsTr("Turn photos and scans into one PDF, one image per page. JPEGs are embedded without quality loss.")
    files: Item {
        DropZone {
            anchors.fill: parent
            visible: images.count === 0
            images: true
            onFilesChosen: (urls) => page.addFiles(urls)
        }
        ColumnLayout {
            anchors.fill: parent
            visible: images.count > 0
            spacing: Theme.spacingS
            RowLayout {
                Label {
                    Layout.fillWidth: true
                    text: qsTr("%n image(s)", "", images.count)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeS
                    font.weight: Theme.weightMedium
                }
                AppButton { compact: true; kind: "ghost"; iconName: "plus"; text: qsTr("Add images"); onClicked: picker.open() }
                AppButton { compact: true; kind: "ghost"; text: qsTr("Clear"); onClicked: images.clear() }
            }
            GridView {
                id: grid
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: images
                cellWidth: 168
                cellHeight: 208
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                move: Transition { NumberAnimation { properties: "x,y"; duration: Theme.durationNormal; easing.type: Easing.OutCubic } }
                displaced: Transition { NumberAnimation { properties: "x,y"; duration: Theme.durationNormal; easing.type: Easing.OutCubic } }
                delegate: Item {
                    id: tile
                    required property int index
                    required property url url
                    required property string name
                    width: grid.cellWidth
                    height: grid.cellHeight
                    HoverHandler { id: hover }
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingXs
                        radius: Theme.radiusCard
                        color: hover.hovered ? Theme.surfaceHover : Theme.surface
                        border.width: 1
                        border.color: Theme.border
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingS
                            spacing: Theme.spacingXs
                            Image {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                source: tile.url
                                sourceSize: Qt.size(160 * Screen.devicePixelRatio, 160 * Screen.devicePixelRatio)
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                autoTransform: true
                            }
                            Label {
                                Layout.fillWidth: true
                                text: (tile.index + 1) + ".  " + tile.name
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeXs
                                elide: Text.ElideMiddle
                            }
                        }
                        Row {
                            anchors.top: parent.top
                            anchors.right: parent.right
                            anchors.margins: Theme.spacingXs
                            visible: hover.hovered
                            spacing: Theme.spacingXxs
                            IconButton { iconName: "chevron-up"; rotation: -90; size: Theme.controlHeightSmall; tip: qsTr("Move earlier"); enabled: tile.index > 0; onClicked: images.move(tile.index, tile.index - 1) }
                            IconButton { iconName: "chevron-down"; rotation: -90; size: Theme.controlHeightSmall; tip: qsTr("Move later"); enabled: tile.index < images.count - 1; onClicked: images.move(tile.index, tile.index + 1) }
                            IconButton { iconName: "x"; size: Theme.controlHeightSmall; danger: true; tip: qsTr("Remove"); onClicked: images.remove(tile.index) }
                        }
                    }
                }
                DropArea {
                    anchors.fill: parent
                    onDropped: (event) => { if (event.hasUrls) page.addFiles(event.urls) }
                }
            }
        }
        FileDialog {
            id: picker
            fileMode: FileDialog.OpenFiles
            nameFilters: [qsTr("Images (*.jpg *.jpeg *.png *.webp *.bmp *.tif *.tiff *.gif)"), qsTr("All files (*)")]
            onAccepted: page.addFiles(selectedFiles)
        }
    }

    actionText: qsTr("Create PDF")
    actionIcon: "file-image"
    actionEnabled: images.count > 0
    onAction: {
        saveDialog.currentFile = Tools.suggestedFile(null, -1, "images")
        saveDialog.open()
    }

    SectionTitle { text: qsTr("Page size") }
    SegmentedControl {
        id: pageSize
        Layout.fillWidth: true
        options: [qsTr("Fit image"), "A4", qsTr("Letter")]
    }
    ColumnLayout {
        Layout.fillWidth: true
        visible: pageSize.currentIndex !== 0
        spacing: Theme.spacingM
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            FieldLabel { text: qsTr("Orientation") }
            SegmentedControl {
                id: orientation
                Layout.fillWidth: true
                options: [qsTr("Automatic"), qsTr("Portrait"), qsTr("Landscape")]
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXs
            FieldLabel { text: qsTr("Margin") }
            SegmentedControl {
                id: margin
                Layout.fillWidth: true
                options: [qsTr("None"), qsTr("Small"), qsTr("Large")]
                currentIndex: 1
            }
        }
    }
    HelpText {
        Layout.fillWidth: true
        text: pageSize.currentIndex === 0
              ? qsTr("Each page is exactly the size of its image.")
              : qsTr("Images are scaled down to fit and centred; they are never enlarged.")
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: Tools.imagesToPdf(images, pageSize.currentIndex, orientation.currentIndex,
                                      [0, 10, 20][margin.currentIndex], selectedFile)
    }
}
