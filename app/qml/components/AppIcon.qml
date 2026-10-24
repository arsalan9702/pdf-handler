import QtQuick
import QtQuick.Window
import PdfHandler

// A Lucide icon tinted with `color`, rendered at the screen's pixel density.
Image {
    required property string name
    property color color: Theme.textSecondary
    property int size: Theme.iconSize

    width: size
    height: size
    source: name.length > 0 ? "image://icon/" + name + "/" + color.toString().slice(-6) : ""
    sourceSize: Qt.size(size * Screen.devicePixelRatio, size * Screen.devicePixelRatio)
    fillMode: Image.PreserveAspectFit
    smooth: true
}
