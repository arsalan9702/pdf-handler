import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// Standard tool page: header, the files on the left, options and the main action on the right.
FocusScope {
    id: root

    required property string title
    property string subtitle: ""
    property Item files
    default property alias options: optionsColumn.data
    property string actionText: ""
    property string actionIcon: ""
    property bool actionEnabled: true
    property string actionHint: ""
    signal action()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingL
        spacing: Theme.spacingL

        PageHeader {
            Layout.fillWidth: true
            title: root.title
            subtitle: root.subtitle
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingL

            Item {
                id: filesHost
                Layout.fillWidth: true
                Layout.fillHeight: true
                data: root.files ? [root.files] : []
                Component.onCompleted: if (root.files) root.files.anchors.fill = filesHost
            }

            Card {
                Layout.preferredWidth: Theme.panelWidth
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingM + Theme.spacingXs
                    spacing: Theme.spacingM

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentWidth: availableWidth
                        clip: true
                        ColumnLayout {
                            id: optionsColumn
                            width: parent.width
                            spacing: Theme.spacingM
                        }
                    }

                    HelpText {
                        Layout.fillWidth: true
                        visible: root.actionHint.length > 0
                        text: root.actionHint
                        horizontalAlignment: Text.AlignHCenter
                    }
                    AppButton {
                        Layout.fillWidth: true
                        kind: "primary"
                        text: root.actionText
                        iconName: root.actionIcon
                        enabled: root.actionEnabled && !Jobs.busy
                        onClicked: root.action()
                    }
                }
            }
        }
    }
}
