import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Window
import PdfHandler

// Large view of one page; arrow keys step through the project.
Popup {
    id: popup

    required property PageProject project
    property int row: 0
    // Re-read whenever the project changes (rotation, reorder) or the row moves.
    readonly property var item: project.revision >= 0 ? project.pageAt(row) : ({})

    function show(index) {
        row = index
        open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: parent ? parent.width * 0.86 : 800
    height: parent ? parent.height * 0.9 : 600
    modal: true
    focus: true
    padding: Theme.spacingM
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Theme.scrim }

    background: Rectangle {
        radius: Theme.radiusCard
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
    }

    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durationNormal } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durationFast } }

    contentItem: ColumnLayout {
        spacing: Theme.spacingS
        focus: true
        Keys.onLeftPressed: if (popup.row > 0) popup.row--
        Keys.onRightPressed: if (popup.row < popup.project.count - 1) popup.row++

        RowLayout {
            Label {
                Layout.fillWidth: true
                text: (popup.item.docName || "") + "  ·  " + qsTr("page %1").arg(popup.item.pageNumber || "")
                      + "  ·  " + qsTr("%1 of %2").arg(popup.row + 1).arg(popup.project.count)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeM
                elide: Text.ElideMiddle
            }
            IconButton { iconName: "rotate-ccw"; tip: qsTr("Rotate left"); onClicked: popup.project.rotate(popup.row, -90) }
            IconButton { iconName: "rotate-cw"; tip: qsTr("Rotate right"); onClicked: popup.project.rotate(popup.row, 90) }
            IconButton { iconName: "x"; tip: qsTr("Close (Esc)"); onClicked: popup.close() }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            Image {
                id: big
                readonly property bool sideways: (popup.item.rotation || 0) % 180 !== 0
                anchors.centerIn: parent
                width: sideways ? parent.height : parent.width
                height: sideways ? parent.width : parent.height
                source: popup.opened && popup.item.thumbnail ? popup.item.thumbnail : ""
                sourceSize: Qt.size(1600 * Screen.devicePixelRatio, 1600 * Screen.devicePixelRatio)
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                rotation: popup.item.rotation || 0
                Behavior on rotation { RotationAnimation { duration: Theme.durationNormal; direction: RotationAnimation.Shortest } }
            }
            BusyIndicator { anchors.centerIn: parent; running: big.status === Image.Loading }
            IconButton {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                iconName: "chevron-up"
                rotation: -90
                tip: qsTr("Previous (←)")
                enabled: popup.row > 0
                onClicked: popup.row--
            }
            IconButton {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                iconName: "chevron-down"
                rotation: -90
                tip: qsTr("Next (→)")
                enabled: popup.row < popup.project.count - 1
                onClicked: popup.row++
            }
        }
    }
}
