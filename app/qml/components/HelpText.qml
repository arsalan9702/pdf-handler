import QtQuick
import QtQuick.Controls.Basic
import PdfHandler

Label {
    property bool error: false
    color: error ? Theme.danger : Theme.textTertiary
    font.pixelSize: Theme.fontSizeS
    wrapMode: Text.Wrap
}
