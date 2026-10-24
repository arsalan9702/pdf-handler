import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// Stack of transient notifications in the bottom-right corner.
Item {
    id: host

    // kind: "success" | "error" | "warning" | "info"
    function show(kind, title, message, path, details) {
        toasts.append({ kind: kind, title: title, message: message || "", path: path || "",
                        details: details || "" })
        if (toasts.count > 4)
            toasts.remove(0)
    }

    ListModel { id: toasts }

    Column {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingL
        spacing: Theme.spacingS
        width: Math.min(400, host.width - 2 * Theme.spacingL)

        Repeater {
            model: toasts
            delegate: Toast {
                width: parent.width
                onDismissed: toasts.remove(index)
            }
        }
    }
}
