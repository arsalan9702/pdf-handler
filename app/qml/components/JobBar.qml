import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts
import PdfHandler

// Progress for the running job, with cancel. Never blocks the rest of the window.
Item {
    id: root

    visible: opacity > 0
    opacity: Jobs.busy ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.durationNormal } }
    implicitHeight: card.implicitHeight

    RectangularShadow {
        anchors.fill: card
        radius: card.radius
        blur: 24
        offset.y: 6
        color: Theme.shadow
    }

    Rectangle {
        id: card
        width: parent.width
        implicitHeight: layout.implicitHeight + 2 * Theme.spacingM
        radius: Theme.radiusCard
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        RowLayout {
            id: layout
            anchors.fill: parent
            anchors.margins: Theme.spacingM
            spacing: Theme.spacingM

            BusyIndicator {
                running: Jobs.busy
                implicitWidth: 24
                implicitHeight: 24
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingS
                RowLayout {
                    Label {
                        Layout.fillWidth: true
                        text: Jobs.title + "…"
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeM
                        font.weight: Theme.weightMedium
                        elide: Text.ElideRight
                    }
                    Label {
                        text: Math.round(Jobs.progress * 100) + "%"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeS
                        font.features: { "tnum": 1 }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 4
                    radius: 2
                    color: Theme.border
                    Rectangle {
                        width: parent.width * Jobs.progress
                        height: parent.height
                        radius: 2
                        color: Theme.accent
                        Behavior on width { NumberAnimation { duration: Theme.durationFast } }
                    }
                }
            }
            AppButton {
                text: qsTr("Cancel")
                compact: true
                onClicked: Jobs.cancel()
            }
        }
    }
}
