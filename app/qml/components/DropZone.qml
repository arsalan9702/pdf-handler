import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import PdfHandler

// Empty-state target: drop files here or pick them with the native dialog.
Rectangle {
    id: root

    property bool images: false
    property bool multiple: true
    property string title: images ? qsTr("Drop images here") : qsTr("Drop PDF files here")
    property string hint: qsTr("or choose files from your computer")
    signal filesChosen(list<url> urls)

    radius: Theme.radiusCard
    color: drop.containsDrag ? Theme.accentSoft : Theme.surface
    border.width: 2
    border.color: drop.containsDrag ? Theme.accent : Theme.border
    Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }

    // Dashed look without an image: overlay a stippled border.
    Canvas {
        anchors.fill: parent
        visible: !drop.containsDrag
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.setLineDash([6, 6])
            ctx.lineWidth = 2
            ctx.strokeStyle = Theme.borderStrong
            ctx.beginPath()
            ctx.roundedRect(1, 1, width - 2, height - 2, Theme.radiusCard, Theme.radiusCard)
            ctx.stroke()
        }
        Connections {
            target: Theme
            function onDarkChanged() { parent.requestPaint() }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacingL, 420)
        spacing: Theme.spacingM

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 56
            height: 56
            radius: 28
            color: Theme.accentSoft
            AppIcon {
                anchors.centerIn: parent
                name: root.images ? "images" : "upload"
                size: Theme.iconSizeLarge
                color: Theme.accent
            }
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeL
            font.weight: Theme.weightSemibold
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: root.hint
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeM
        }
        AppButton {
            Layout.alignment: Qt.AlignHCenter
            kind: "primary"
            iconName: "folder-open"
            text: qsTr("Choose files")
            onClicked: picker.open()
        }
    }

    DropArea {
        id: drop
        anchors.fill: parent
        onDropped: (event) => {
            if (event.hasUrls)
                root.filesChosen(event.urls)
        }
    }

    FileDialog {
        id: picker
        fileMode: root.multiple ? FileDialog.OpenFiles : FileDialog.OpenFile
        nameFilters: root.images
            ? [qsTr("Images (*.jpg *.jpeg *.png *.webp *.bmp *.tif *.tiff *.gif)"), qsTr("All files (*)")]
            : [qsTr("PDF files (*.pdf)"), qsTr("All files (*)")]
        onAccepted: root.filesChosen(root.multiple ? selectedFiles : [selectedFile])
    }

    function choose() { picker.open() }
}
