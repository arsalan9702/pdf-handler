import QtQuick
import QtQuick.Controls.Basic
import PdfHandler

TextField {
    id: control

    property string errorText: ""
    readonly property bool hasError: errorText.length > 0

    implicitHeight: Theme.controlHeight
    leftPadding: Theme.spacingS + Theme.spacingXs
    rightPadding: leftPadding
    font.pixelSize: Theme.fontSizeM
    color: Theme.textPrimary
    placeholderTextColor: Theme.textTertiary
    selectionColor: Theme.accent
    selectedTextColor: Theme.textOnAccent
    selectByMouse: true

    background: Rectangle {
        radius: Theme.radiusControl
        color: control.enabled ? Theme.inputBackground : Theme.surfaceAlt
        border.width: control.activeFocus || control.hasError ? 2 : 1
        border.color: control.hasError ? Theme.danger
                    : control.activeFocus ? Theme.accent : Theme.borderStrong
        Behavior on border.color { ColorAnimation { duration: Theme.durationFast } }
    }
}
