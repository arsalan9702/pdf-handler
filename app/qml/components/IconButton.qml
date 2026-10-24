import QtQuick
import QtQuick.Controls.Basic
import PdfHandler

// Square icon-only button with a tooltip.
ToolButton {
    id: control

    required property string iconName
    property string tip: ""
    property bool danger: false
    property int size: Theme.controlHeight

    implicitWidth: size
    implicitHeight: size
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    ToolTip.visible: hovered && tip.length > 0
    ToolTip.delay: 500
    ToolTip.text: tip

    HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }

    contentItem: Item {
        AppIcon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.size <= Theme.controlHeightSmall ? Theme.iconSizeSmall : Theme.iconSize
            color: !control.enabled ? Theme.textTertiary
                 : control.danger && control.hovered ? Theme.danger
                 : control.checked ? Theme.accent : Theme.textSecondary
        }
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.down ? Theme.surfacePressed
             : control.checked ? Theme.accentSoft
             : control.hovered ? Theme.surfaceHover : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }
}
