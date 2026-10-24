import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// Start screen: every tool at a glance; dropping PDFs here opens them in the organizer.
Item {
    id: page

    required property var tools
    signal toolRequested(string key)
    signal filesDropped(list<url> urls)

    DropArea {
        id: drop
        anchors.fill: parent
        onDropped: (event) => { if (event.hasUrls) page.filesDropped(event.urls) }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: Theme.spacingS
        radius: Theme.radiusCard
        color: "transparent"
        border.width: 2
        border.color: Theme.accent
        visible: drop.containsDrag
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(parent.width - 2 * Theme.spacingXl, Theme.contentMaxWidth)
            x: (parent.width - width) / 2
            spacing: Theme.spacingL

            Item { Layout.preferredHeight: Theme.spacingL }

            ColumnLayout {
                spacing: Theme.spacingS
                Label {
                    text: qsTr("What would you like to do?")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeXxl
                    font.weight: Theme.weightSemibold
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Everything happens on your computer; files are never uploaded. Originals are never modified unless you save over them, and every result is verified before it's written.")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeM
                    wrapMode: Text.Wrap
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: Math.max(1, Math.floor((width + columnSpacing) / (240 + columnSpacing)))
                columnSpacing: Theme.spacingM
                rowSpacing: Theme.spacingM

                Repeater {
                    model: page.tools.filter(t => t.key !== "home" && t.key !== "settings")
                    delegate: ToolCard {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 240
                        text: modelData.title
                        description: modelData.description
                        iconName: modelData.icon
                        tint: modelData.tint
                        onClicked: page.toolRequested(modelData.key)
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.bottomMargin: Theme.spacingL
                text: qsTr("Tip: drop PDF files anywhere on this screen to start organizing them.")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeS
            }
        }
    }
}
