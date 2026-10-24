import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// Selectable card with a title and description (radio-style), e.g. compression presets.
AbstractButton {
    id: card

    required property string description
    property string iconName: ""

    checkable: true
    hoverEnabled: true
    implicitHeight: layout.implicitHeight + 2 * Theme.spacingS + Theme.spacingXs

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    background: Rectangle {
        radius: Theme.radiusControl
        color: card.checked ? Theme.accentSoft : card.hovered ? Theme.surfaceHover : Theme.surface
        border.width: card.checked ? 2 : 1
        border.color: card.checked ? Theme.accent : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }

    contentItem: RowLayout {
        id: layout
        spacing: Theme.spacingS + Theme.spacingXs
        Rectangle {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 2
            width: 18; height: 18; radius: 9
            color: "transparent"
            border.width: card.checked ? 5 : 2
            border.color: card.checked ? Theme.accent : Theme.borderStrong
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingXxs
            Label {
                Layout.fillWidth: true
                text: card.text
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeM
                font.weight: Theme.weightMedium
            }
            Label {
                Layout.fillWidth: true
                text: card.description
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeS
                wrapMode: Text.Wrap
            }
        }
    }
}
