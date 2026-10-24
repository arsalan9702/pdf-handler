import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// Title row at the top of each tool page; children go on the right.
RowLayout {
    id: root

    required property string title
    property string subtitle: ""
    default property alias actions: actionRow.data

    spacing: Theme.spacingM

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingXs
        Label {
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeXl
            font.weight: Theme.weightSemibold
        }
        Label {
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeM
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }

    RowLayout {
        id: actionRow
        spacing: Theme.spacingS
        Layout.alignment: Qt.AlignTop
    }
}
