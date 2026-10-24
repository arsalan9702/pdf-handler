import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// Sidebar entry.
AbstractButton {
    id: item

    required property string iconName
    property bool current: false

    implicitHeight: Theme.controlHeight
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    contentItem: RowLayout {
        spacing: Theme.spacingS + Theme.spacingXs
        AppIcon {
            Layout.leftMargin: Theme.spacingS + Theme.spacingXs
            name: item.iconName
            color: item.current ? Theme.accent : Theme.textSecondary
        }
        Label {
            Layout.fillWidth: true
            text: item.text
            color: item.current ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Theme.fontSizeM
            font.weight: item.current ? Theme.weightSemibold : Theme.weightMedium
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: item.current ? Theme.surface : item.down ? Theme.surfacePressed
             : item.hovered ? Theme.surfaceHover : "transparent"
        border.width: item.current || item.visualFocus ? 1 : 0
        border.color: item.visualFocus ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }
}
