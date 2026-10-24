import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import PdfHandler

// Organize pages: reorder, rotate, delete, duplicate, combine files, extract a selection.
// Nothing is written until "Save as…".
FocusScope {
    id: page

    property alias documents: docs
    readonly property bool active: visible

    function addFiles(urls) { docs.addUrls(urls) }

    DocumentList { id: docs }
    PageProject { id: project; documents: docs }

    property int thumbSize: 168
    property int dragFrom: -1
    property int dropTarget: -1   // index the drag is over
    property bool dropAfter: false

    // Keyboard shortcuts while this page is shown.
    Shortcut { enabled: page.active; sequences: [StandardKey.Undo]; onActivated: project.undo() }
    Shortcut { enabled: page.active; sequences: [StandardKey.Redo, "Ctrl+Y"]; onActivated: project.redo() }
    Shortcut { enabled: page.active; sequences: [StandardKey.SelectAll]; onActivated: project.selectAll() }
    Shortcut { enabled: page.active; sequences: [StandardKey.Delete, "Backspace"]; onActivated: project.remove(-1) }
    Shortcut { enabled: page.active; sequences: [StandardKey.Save, StandardKey.SaveAs]; onActivated: saveAll() }
    Shortcut { enabled: page.active; sequence: "Ctrl+R"; onActivated: project.rotate(-1, 90) }
    Shortcut { enabled: page.active; sequence: "Ctrl+Shift+R"; onActivated: project.rotate(-1, -90) }
    Shortcut { enabled: page.active; sequence: "Escape"; onActivated: project.clearSelection() }

    property bool saveSelectionOnly: false
    function saveAll() {
        if (project.count === 0) return
        saveSelectionOnly = false
        saveDialog.currentFile = Tools.suggestedFile(docs, 0, docs.count > 1 ? "-combined" : "-edited")
        saveDialog.open()
    }
    function saveSelected() {
        saveSelectionOnly = true
        saveDialog.currentFile = Tools.suggestedFile(docs, 0, "-selection")
        saveDialog.open()
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: Tools.exportPages(project, selectedFile, page.saveSelectionOnly)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingL
        spacing: Theme.spacingM

        PageHeader {
            Layout.fillWidth: true
            title: qsTr("Organize pages")
            subtitle: docs.count === 0
                ? qsTr("Reorder, rotate, delete and duplicate pages, or combine several PDFs into one.")
                : qsTr("%n page(s)", "", project.count)
                  + (project.selectedCount > 0 ? "  ·  " + qsTr("%n selected", "", project.selectedCount) : "")
                  + "  ·  " + qsTr("Drag to reorder, double-click to preview")

            AppButton {
                visible: docs.count > 0
                iconName: "plus"
                text: qsTr("Add PDF")
                onClicked: addDialog.open()
            }
            AppButton {
                visible: project.selectedCount > 0
                iconName: "file-plus"
                text: qsTr("Save selection…")
                enabled: !Jobs.busy
                onClicked: page.saveSelected()
            }
            AppButton {
                visible: docs.count > 0
                kind: "primary"
                iconName: "download"
                text: qsTr("Save as…")
                enabled: project.count > 0 && docs.allReady && !Jobs.busy
                onClicked: page.saveAll()
            }
        }

        // Toolbar
        Card {
            Layout.fillWidth: true
            visible: docs.count > 0
            implicitHeight: toolbar.implicitHeight + 2 * Theme.spacingXs
            RowLayout {
                id: toolbar
                anchors.fill: parent
                anchors.margins: Theme.spacingXs
                spacing: Theme.spacingXxs

                IconButton { iconName: "undo-2"; tip: project.canUndo ? qsTr("Undo %1").arg(project.undoText) : qsTr("Undo"); enabled: project.canUndo; onClicked: project.undo() }
                IconButton { iconName: "redo-2"; tip: project.canRedo ? qsTr("Redo %1").arg(project.redoText) : qsTr("Redo"); enabled: project.canRedo; onClicked: project.redo() }
                Rectangle { width: 1; height: 20; color: Theme.border; Layout.leftMargin: Theme.spacingXs; Layout.rightMargin: Theme.spacingXs }
                IconButton { iconName: "rotate-ccw"; tip: qsTr("Rotate left (Ctrl+Shift+R)"); enabled: project.selectedCount > 0; onClicked: project.rotate(-1, -90) }
                IconButton { iconName: "rotate-cw"; tip: qsTr("Rotate right (Ctrl+R)"); enabled: project.selectedCount > 0; onClicked: project.rotate(-1, 90) }
                IconButton { iconName: "copy"; tip: qsTr("Duplicate"); enabled: project.selectedCount > 0; onClicked: project.duplicate(-1) }
                IconButton { iconName: "trash-2"; tip: qsTr("Delete (Del)"); danger: true; enabled: project.selectedCount > 0; onClicked: project.remove(-1) }
                Rectangle { width: 1; height: 20; color: Theme.border; Layout.leftMargin: Theme.spacingXs; Layout.rightMargin: Theme.spacingXs }
                IconButton { iconName: "square-check-big"; tip: qsTr("Select all (Ctrl+A)"); onClicked: project.selectAll() }
                IconButton { iconName: "square"; tip: qsTr("Clear selection (Esc)"); enabled: project.selectedCount > 0; onClicked: project.clearSelection() }
                AppButton { compact: true; kind: "ghost"; text: qsTr("Reverse order"); onClicked: project.reverse() }

                Item { Layout.fillWidth: true }

                AppIcon { name: "image"; size: Theme.iconSizeSmall }
                AppSlider {
                    Layout.preferredWidth: 140
                    from: 112; to: 320; stepSize: 8
                    value: page.thumbSize
                    onMoved: page.thumbSize = value
                }
            }
        }

        // Files in this project (colour key when combining several).
        Flow {
            Layout.fillWidth: true
            visible: docs.count > 1 || (docs.count === 1 && !docs.allReady)
            spacing: Theme.spacingS
            Repeater {
                model: docs
                delegate: Rectangle {
                    required property int index
                    required property string name
                    required property int status
                    required property int pageCount
                    height: Theme.controlHeightSmall
                    width: chip.implicitWidth + Theme.spacingM
                    radius: height / 2
                    color: Theme.surface
                    border.width: 1
                    border.color: status === DocumentList.Ready ? Theme.border : Theme.warning
                    RowLayout {
                        id: chip
                        anchors.centerIn: parent
                        spacing: Theme.spacingXs
                        Rectangle { width: 8; height: 8; radius: 4; color: Theme.docColors[index % Theme.docColors.length] }
                        Label {
                            text: name + (status === DocumentList.Ready ? "  ·  " + pageCount
                                  : status === DocumentList.Loading ? "  ·  " + qsTr("opening…") : "  ·  " + qsTr("needs attention"))
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSizeS
                        }
                        IconButton { iconName: "x"; size: 20; tip: qsTr("Remove this file and its pages"); onClicked: docs.remove(index) }
                    }
                }
            }
        }

        // Locked or unreadable files: unlock or remove them right here.
        Column {
            Layout.fillWidth: true
            spacing: Theme.spacingS
            Repeater {
                model: docs
                delegate: DocumentRow {
                    width: parent.width
                    documents: docs
                    visible: status !== DocumentList.Ready && status !== DocumentList.Loading
                    height: visible ? implicitHeight : 0
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            DropZone {
                anchors.fill: parent
                visible: docs.count === 0
                title: qsTr("Drop PDF files to start")
                hint: qsTr("Several files are combined in the order you add them")
                onFilesChosen: (urls) => docs.addUrls(urls)
            }

            GridView {
                id: grid
                anchors.fill: parent
                visible: project.count > 0
                clip: true
                model: project
                cellWidth: page.thumbSize + Theme.spacingM
                cellHeight: page.thumbSize + Theme.spacingS + (docs.count > 1 ? Theme.spacingL : Theme.spacingS)
                cacheBuffer: cellHeight * 2
                boundsBehavior: Flickable.StopAtBounds
                reuseItems: true
                focus: true
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: PageTile {
                    project: project
                    thumbSize: page.thumbSize
                    multiDoc: docs.count > 1
                    dropIndicator: page.dragFrom >= 0 && page.dropTarget === index && !selected
                                   ? (page.dropAfter ? 1 : -1) : 0
                    onPreviewRequested: preview.show(index)
                    onContextMenuRequested: (x, y) => { contextMenu.row = index; contextMenu.popup() }
                    onDragStarted: page.dragFrom = index
                    onDragMoved: (scenePos) => page.updateDrop(scenePos)
                    onDragFinished: page.finishDrag()
                }

                // Click on empty space clears the selection.
                TapHandler {
                    onTapped: (eventPoint) => {
                        const p = eventPoint.position
                        if (grid.indexAt(p.x + grid.contentX, p.y + grid.contentY) < 0)
                            project.clearSelection()
                    }
                }

                DropArea {
                    anchors.fill: parent
                    onDropped: (event) => { if (event.hasUrls) docs.addUrls(event.urls) }
                }

                // Auto-scroll while dragging near the top or bottom edge.
                Timer {
                    id: autoScroll
                    property real speed: 0
                    interval: 16
                    repeat: true
                    running: page.dragFrom >= 0 && speed !== 0
                    onTriggered: grid.contentY = Math.max(0, Math.min(grid.contentHeight - grid.height, grid.contentY + speed))
                }
            }
        }
    }

    function updateDrop(scenePos) {
        const p = grid.mapFromItem(null, scenePos.x, scenePos.y)
        const edge = 48
        autoScroll.speed = p.y < edge ? -12 : p.y > grid.height - edge ? 12 : 0
        let idx = grid.indexAt(p.x + grid.contentX, p.y + grid.contentY)
        if (idx < 0) {
            // Past the last tile: drop at the end.
            idx = project.count - 1
            dropAfter = true
        } else {
            const cellX = (p.x + grid.contentX) % grid.cellWidth
            dropAfter = cellX > grid.cellWidth / 2
        }
        dropTarget = idx
    }

    function finishDrag() {
        if (dragFrom >= 0 && dropTarget >= 0)
            project.moveTo(dragFrom, dropTarget + (dropAfter ? 1 : 0))
        dragFrom = -1
        dropTarget = -1
        autoScroll.speed = 0
    }

    Menu {
        id: contextMenu
        property int row: -1
        MenuItem { text: qsTr("Rotate right"); onTriggered: project.rotate(contextMenu.row, 90) }
        MenuItem { text: qsTr("Rotate left"); onTriggered: project.rotate(contextMenu.row, -90) }
        MenuItem { text: qsTr("Rotate 180°"); onTriggered: project.rotate(contextMenu.row, 180) }
        MenuSeparator {}
        MenuItem { text: qsTr("Duplicate"); onTriggered: project.duplicate(contextMenu.row) }
        MenuItem { text: qsTr("Save selection as…"); onTriggered: page.saveSelected() }
        MenuSeparator {}
        MenuItem { text: qsTr("Delete"); onTriggered: project.remove(contextMenu.row) }
    }

    FileDialog {
        id: addDialog
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: docs.addUrls(selectedFiles)
    }

    PagePreview {
        id: preview
        project: project
    }
}
