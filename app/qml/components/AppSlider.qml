import QtQuick
import QtQuick.Controls.Basic
import PdfHandler

Slider {
    id: control

    implicitHeight: Theme.controlHeight

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4
        radius: 2
        color: Theme.border
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.accent
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 16
        height: 16
        radius: 8
        color: Theme.surface
        border.width: 2
        border.color: Theme.accent
        scale: control.pressed ? 1.15 : 1
        Behavior on scale { NumberAnimation { duration: Theme.durationFast } }
    }
}
