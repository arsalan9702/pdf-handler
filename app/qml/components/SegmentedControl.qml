import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// A row of mutually exclusive options. `options` is a list of labels.
Rectangle {
    id: root

    required property var options
    property int currentIndex: 0
    signal activated(int index)

    implicitHeight: Theme.controlHeight
    implicitWidth: row.implicitWidth + 2 * Theme.spacingXs
    radius: Theme.radiusControl
    color: Theme.surfaceAlt
    border.width: 1
    border.color: Theme.border

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: Theme.spacingXs - 1
        spacing: Theme.spacingXxs

        Repeater {
            model: root.options
            delegate: AbstractButton {
                id: option
                required property int index
                required property string modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                implicitWidth: label.implicitWidth + 2 * Theme.spacingM
                hoverEnabled: true
                checked: root.currentIndex === index
                onClicked: {
                    root.currentIndex = index
                    root.activated(index)
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                contentItem: Label {
                    id: label
                    text: option.modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: Theme.fontSizeS
                    font.weight: Theme.weightMedium
                    color: option.checked ? Theme.textPrimary : Theme.textSecondary
                    elide: Text.ElideRight
                }
                background: Rectangle {
                    radius: Theme.radiusControl - 2
                    color: option.checked ? Theme.surface : option.hovered ? Theme.surfaceHover : "transparent"
                    border.width: option.checked ? 1 : 0
                    border.color: Theme.border
                    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
                }
            }
        }
    }
}
