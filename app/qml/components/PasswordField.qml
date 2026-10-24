import QtQuick
import QtQuick.Controls.Basic
import PdfHandler

AppTextField {
    id: field
    property bool revealed: false
    echoMode: revealed ? TextInput.Normal : TextInput.Password
    rightPadding: eye.width + Theme.spacingS

    IconButton {
        id: eye
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: Theme.spacingXs
        size: Theme.controlHeightSmall
        iconName: "eye"
        checkable: true
        checked: field.revealed
        tip: field.revealed ? qsTr("Hide password") : qsTr("Show password")
        onToggled: field.revealed = checked
    }
}
