import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts
import PdfHandler

Item {
    id: toast

    required property int index
    required property string kind
    required property string title
    required property string message
    required property string path
    required property string details
    signal dismissed()

    readonly property color tone: kind === "error" ? Theme.danger
                                : kind === "warning" ? Theme.warning
                                : kind === "success" ? Theme.success : Theme.accent

    implicitHeight: card.implicitHeight
    opacity: 0
    Component.onCompleted: opacity = 1
    Behavior on opacity { NumberAnimation { duration: Theme.durationNormal } }

    Timer {
        // Errors and results with details stay until dismissed.
        running: toast.kind === "success" || toast.kind === "info"
        interval: Theme.toastDuration
        onTriggered: toast.dismissed()
    }

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
        implicitHeight: body.implicitHeight + 2 * Theme.spacingM
        radius: Theme.radiusCard
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        Rectangle {
            width: 3
            radius: 2
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: Theme.spacingS
            color: toast.tone
        }

        RowLayout {
            id: body
            anchors.fill: parent
            anchors.margins: Theme.spacingM
            anchors.leftMargin: Theme.spacingM + Theme.spacingS
            spacing: Theme.spacingS + Theme.spacingXs

            AppIcon {
                Layout.alignment: Qt.AlignTop
                name: toast.kind === "error" ? "circle-x"
                    : toast.kind === "warning" ? "triangle-alert"
                    : toast.kind === "success" ? "circle-check" : "info"
                color: toast.tone
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXs
                Label {
                    Layout.fillWidth: true
                    text: toast.title
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeM
                    font.weight: Theme.weightSemibold
                    wrapMode: Text.Wrap
                }
                Label {
                    Layout.fillWidth: true
                    visible: toast.message.length > 0
                    text: toast.message
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeS
                    wrapMode: Text.Wrap
                    maximumLineCount: 6
                    elide: Text.ElideRight
                }
                Label {
                    id: detailText
                    Layout.fillWidth: true
                    visible: false
                    text: toast.details
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                    wrapMode: Text.Wrap
                }
                RowLayout {
                    visible: toast.path.length > 0 || toast.details.length > 0
                    spacing: Theme.spacingXs
                    AppButton {
                        visible: toast.path.length > 0
                        compact: true
                        kind: "ghost"
                        iconName: "external-link"
                        text: qsTr("Open")
                        onClicked: Tools.openFile(toast.path)
                    }
                    AppButton {
                        visible: toast.path.length > 0
                        compact: true
                        kind: "ghost"
                        iconName: "folder-open"
                        text: qsTr("Show in folder")
                        onClicked: Tools.reveal(toast.path)
                    }
                    AppButton {
                        visible: toast.details.length > 0
                        compact: true
                        kind: "ghost"
                        text: detailText.visible ? qsTr("Hide details") : qsTr("Details")
                        onClicked: detailText.visible = !detailText.visible
                    }
                }
            }

            IconButton {
                Layout.alignment: Qt.AlignTop
                iconName: "x"
                size: Theme.controlHeightSmall
                tip: qsTr("Dismiss")
                onClicked: toast.dismissed()
            }
        }
    }
}
