import QtQuick
import QtQuick.Controls.Basic
import PdfHandler

ApplicationWindow {
    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: qsTr("PDF Handler")
    color: Theme.background

    Behavior on color { ColorAnimation { duration: Theme.durationNormal } }

    Label {
        anchors.centerIn: parent
        text: qsTr("PDF Handler")
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeXl
    }
}
