import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// Home screen tile for one tool.
AbstractButton {
    id: card

    required property string iconName
    required property string description
    property color tint: Theme.accent

    implicitHeight: 140
    hoverEnabled: true

    HoverHandler { cursorShape: Qt.PointingHandCursor }

    background: Rectangle {
        radius: Theme.radiusCard
        color: card.down ? Theme.surfacePressed : Theme.surface
        border.width: 1
        border.color: card.hovered || card.visualFocus ? Theme.accent : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacingS
        Rectangle {
            Layout.leftMargin: Theme.spacingM
            Layout.topMargin: Theme.spacingM
            width: 40
            height: 40
            radius: Theme.radiusControl
            color: Qt.alpha(card.tint, Theme.dark ? 0.18 : 0.10)
            AppIcon {
                anchors.centerIn: parent
                name: card.iconName
                color: card.tint
                size: Theme.iconSizeLarge - 2
            }
        }
        Label {
            Layout.leftMargin: Theme.spacingM
            Layout.rightMargin: Theme.spacingM
            Layout.fillWidth: true
            text: card.text
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeM
            font.weight: Theme.weightSemibold
        }
        Label {
            Layout.leftMargin: Theme.spacingM
            Layout.rightMargin: Theme.spacingM
            Layout.bottomMargin: Theme.spacingM
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: card.description
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeS
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignTop
        }
    }
}
