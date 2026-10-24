import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// kind: "primary" | "secondary" | "ghost" | "danger"
Button {
    id: control

    property string kind: "secondary"
    property string iconName: ""
    property bool compact: false

    readonly property color fillColor: {
        if (!enabled)
            return kind === "primary" || kind === "danger" ? Theme.surfacePressed : "transparent"
        switch (kind) {
        case "primary":
            return down ? Theme.accentPressed : hovered ? Theme.accentHover : Theme.accent
        case "danger":
            return down ? Qt.darker(Theme.danger, 1.15) : hovered ? Qt.lighter(Theme.danger, 1.08) : Theme.danger
        case "ghost":
            return down ? Theme.surfacePressed : hovered ? Theme.surfaceHover : "transparent"
        default:
            return down ? Theme.surfacePressed : hovered ? Theme.surfaceHover : Theme.surface
        }
    }
    readonly property color textColor: !enabled ? Theme.textTertiary
        : (kind === "primary" || kind === "danger") ? Theme.textOnAccent : Theme.textPrimary

    implicitHeight: compact ? Theme.controlHeightSmall : Theme.controlHeight
    implicitWidth: Math.max(implicitHeight, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: text.length > 0 ? (compact ? Theme.spacingS + Theme.spacingXs : Theme.spacingM) : 0
    rightPadding: leftPadding
    font.pixelSize: compact ? Theme.fontSizeS : Theme.fontSizeM
    font.weight: Theme.weightMedium
    hoverEnabled: true

    HoverHandler { cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor }

    contentItem: RowLayout {
        spacing: Theme.spacingS
        Item { Layout.fillWidth: control.text.length === 0; visible: control.text.length === 0 }
        AppIcon {
            visible: control.iconName.length > 0
            name: control.iconName
            color: control.textColor
            size: control.compact ? Theme.iconSizeSmall : Theme.iconSize
        }
        Label {
            visible: control.text.length > 0
            text: control.text
            font: control.font
            color: control.textColor
            elide: Text.ElideRight
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
        Item { Layout.fillWidth: control.text.length === 0; visible: control.text.length === 0 }
    }

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.fillColor
        border.width: control.kind === "secondary" ? 1 : 0
        border.color: control.visualFocus ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }
}
