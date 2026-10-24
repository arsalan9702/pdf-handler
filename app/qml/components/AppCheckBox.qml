import QtQuick
import QtQuick.Controls.Basic
import PdfHandler

CheckBox {
    id: control

    font.pixelSize: Theme.fontSizeM
    spacing: Theme.spacingS
    hoverEnabled: true

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 4
        color: control.checked ? Theme.accent : Theme.inputBackground
        border.width: control.checked ? 0 : 1
        border.color: control.hovered ? Theme.accent : Theme.borderStrong
        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
        AppIcon {
            anchors.centerIn: parent
            name: "check"
            size: 14
            color: Theme.textOnAccent
            visible: control.checked
        }
    }

    contentItem: Label {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: control.enabled ? Theme.textPrimary : Theme.textTertiary
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
    }
}
