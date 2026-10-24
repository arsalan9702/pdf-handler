import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import PdfHandler

// A page thumbnail in the organizer grid: select, drag to reorder, rotate with animation.
Item {
    id: cell
    objectName: "pageTile"

    required property int index
    required property string thumbnail
    required property int pageRotation // degrees, clockwise
    required property int pageNumber
    required property string docName
    required property int docIndex
    required property bool selected
    required property PageProject project
    required property int thumbSize
    property bool multiDoc: false
    property int dropIndicator: 0 // -1 before, 1 after, 0 none

    signal previewRequested()
    signal contextMenuRequested(real x, real y)
    signal dragStarted()
    signal dragMoved(point scenePos)
    signal dragFinished()

    width: GridView.view ? GridView.view.cellWidth : thumbSize
    height: GridView.view ? GridView.view.cellHeight : thumbSize

    Rectangle {
        // Insertion marker while dragging.
        width: 3
        radius: 2
        color: Theme.accent
        visible: cell.dropIndicator !== 0
        x: cell.dropIndicator < 0 ? 1 : cell.width - 4
        y: Theme.spacingS
        height: cell.height - 2 * Theme.spacingS
    }

    Item {
        id: tile
        width: cell.width
        height: cell.height
        opacity: dragHandler.active ? 0.6 : 1

        Rectangle {
            id: frame
            anchors.horizontalCenter: parent.horizontalCenter
            y: Theme.spacingS
            width: cell.thumbSize
            height: cell.thumbSize
            radius: Theme.radiusCard
            color: cell.selected ? Theme.accentSoft : hover.hovered ? Theme.surfaceHover : "transparent"
            border.width: cell.selected ? 2 : 0
            border.color: Theme.selection
            Behavior on color { ColorAnimation { duration: Theme.durationFast } }

            Image {
                id: page
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.spacingM
                height: parent.height - 2 * Theme.spacingM
                source: cell.thumbnail
                sourceSize: Qt.size(cell.thumbSize * Screen.devicePixelRatio,
                                    cell.thumbSize * Screen.devicePixelRatio)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                cache: false // the provider keeps its own LRU cache
                rotation: cell.pageRotation
                Behavior on rotation { RotationAnimation { duration: Theme.durationNormal; direction: RotationAnimation.Shortest } }

                Rectangle {
                    // Paper edge and placeholder while rendering.
                    anchors.centerIn: parent
                    width: page.paintedWidth > 0 ? page.paintedWidth : parent.width * 0.75
                    height: page.paintedHeight > 0 ? page.paintedHeight : parent.height
                    color: page.status === Image.Ready ? "transparent" : Theme.surfaceAlt
                    border.width: 1
                    border.color: Theme.border
                    z: -1
                }
            }

            Rectangle {
                // Page number badge.
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.spacingXs
                height: 20
                width: badge.implicitWidth + 2 * Theme.spacingS
                radius: 10
                color: cell.selected ? Theme.selection : Theme.surface
                border.width: cell.selected ? 0 : 1
                border.color: Theme.border
                RowLayout {
                    id: badge
                    anchors.centerIn: parent
                    spacing: Theme.spacingXs
                    Rectangle {
                        visible: cell.multiDoc
                        width: 8; height: 8; radius: 4
                        color: Theme.docColors[cell.docIndex % Theme.docColors.length]
                    }
                    Label {
                        text: cell.pageNumber
                        color: cell.selected ? Theme.textOnAccent : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Theme.weightSemibold
                    }
                }
            }

            // Quick actions on hover.
            Row {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.spacingXs
                spacing: Theme.spacingXxs
                visible: hover.hovered && !dragHandler.active
                Repeater {
                    model: [
                        { icon: "rotate-ccw", tip: qsTr("Rotate left"), action: "left" },
                        { icon: "rotate-cw", tip: qsTr("Rotate right"), action: "right" },
                        { icon: "trash-2", tip: qsTr("Delete"), action: "delete" }
                    ]
                    delegate: IconButton {
                        required property var modelData
                        objectName: modelData.action + "Button"
                        iconName: modelData.icon
                        tip: modelData.tip
                        size: Theme.controlHeightSmall
                        danger: modelData.action === "delete"
                        background: Rectangle {
                            radius: Theme.radiusControl
                            color: parent.hovered ? Theme.surfaceHover : Theme.surface
                            border.width: 1
                            border.color: Theme.border
                        }
                        onClicked: {
                            if (modelData.action === "left") cell.project.rotate(cell.index, -90)
                            else if (modelData.action === "right") cell.project.rotate(cell.index, 90)
                            else cell.project.remove(cell.index)
                        }
                    }
                }
            }
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: frame.bottom
            anchors.topMargin: Theme.spacingXs
            width: cell.thumbSize
            visible: cell.multiDoc
            text: cell.docName
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
            elide: Text.ElideMiddle
            horizontalAlignment: Text.AlignHCenter
        }

        HoverHandler { id: hover }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: (eventPoint, button) => {
                const mods = point.modifiers
                cell.project.select(cell.index,
                                    (mods & Qt.ControlModifier) || (mods & Qt.MetaModifier),
                                    mods & Qt.ShiftModifier)
            }
            onDoubleTapped: cell.previewRequested()
        }
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: (eventPoint) => {
                if (!cell.selected)
                    cell.project.select(cell.index, false, false)
                cell.contextMenuRequested(eventPoint.position.x, eventPoint.position.y)
            }
        }
        DragHandler {
            id: dragHandler
            target: null
            dragThreshold: 8
            onActiveChanged: {
                if (active) {
                    if (!cell.selected)
                        cell.project.select(cell.index, false, false)
                    cell.dragStarted()
                } else {
                    cell.dragFinished()
                }
            }
            onCentroidChanged: if (active) cell.dragMoved(centroid.scenePosition)
        }
    }
}
