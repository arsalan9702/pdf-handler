import QtCore
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

ApplicationWindow {
    id: window

    width: settings.width
    height: settings.height
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: qsTr("PDF Handler")
    color: Theme.background
    font.family: Theme.fontFamily

    readonly property var tools: [
        { key: "home", title: qsTr("Home"), icon: "house", group: "" },
        { key: "organize", title: qsTr("Organize pages"), icon: "layout-grid", group: qsTr("Pages"), tint: "#2f6bf2",
          description: qsTr("Reorder, rotate, delete, duplicate or extract pages. Combine several PDFs.") },
        { key: "merge", title: qsTr("Merge"), icon: "files", group: qsTr("Pages"), tint: "#8250df",
          description: qsTr("Join several PDFs into one, optionally with page ranges per file.") },
        { key: "split", title: qsTr("Split"), icon: "scissors", group: qsTr("Pages"), tint: "#e5534b",
          description: qsTr("Break a PDF into single pages, fixed-size parts or custom ranges.") },
        { key: "compress", title: qsTr("Compress"), icon: "shrink", group: qsTr("Optimize"), tint: "#2da44e",
          description: qsTr("Shrink files for email and uploads, losslessly or by re-encoding photos.") },
        { key: "protect", title: qsTr("Protect"), icon: "lock", group: qsTr("Security"), tint: "#bf8700",
          description: qsTr("Add a password with AES-256 encryption and set permissions.") },
        { key: "unlock", title: qsTr("Unlock"), icon: "lock-open", group: qsTr("Security"), tint: "#d4558a",
          description: qsTr("Remove the password and restrictions from PDFs you can open.") },
        { key: "properties", title: qsTr("Properties"), icon: "file-pen", group: qsTr("Security"), tint: "#6e7781",
          description: qsTr("Edit the title, author, subject and keywords.") },
        { key: "images2pdf", title: qsTr("Images to PDF"), icon: "images", group: qsTr("Convert"), tint: "#1b9aaa",
          description: qsTr("Turn photos and scans into a PDF, one image per page.") },
        { key: "pdf2images", title: qsTr("PDF to images"), icon: "file-image", group: qsTr("Convert"), tint: "#d97706",
          description: qsTr("Save pages as PNG or JPEG at the resolution you need.") },
        { key: "settings", title: qsTr("Settings"), icon: "settings", group: "" }
    ]
    property string currentKey: "home"
    readonly property int currentIndex: tools.findIndex(t => t.key === currentKey)

    function navigate(key) { currentKey = key }
    function pageFor(key) { return stack.children[tools.findIndex(t => t.key === key)] }
    function toast(kind, title, message, path, details) { toasts.show(kind, title, message, path, details) }

    // Files from the command line, "Open with", Finder, or a second launch.
    function openExternalFiles() {
        const urls = App.takePendingFiles()
        if (urls.length === 0)
            return
        const isImage = u => /\.(jpe?g|png|webp|bmp|gif|tiff?)$/i.test(u.toString())
        const images = urls.filter(isImage)
        const pdfs = urls.filter(u => !isImage(u))
        if (pdfs.length > 0) {
            pageFor("organize").addFiles(pdfs)
            navigate("organize")
        } else {
            pageFor("images2pdf").addFiles(images)
            navigate("images2pdf")
        }
    }

    Settings {
        id: settings
        property string themeMode: "system"
        property int width: 1240
        property int height: 800
    }
    Component.onCompleted: {
        Theme.mode = settings.themeMode
        openExternalFiles()
    }
    onClosing: {
        settings.width = width
        settings.height = height
    }
    Connections {
        target: Theme
        function onModeChanged() { settings.themeMode = Theme.mode }
    }
    Connections {
        target: App
        function onFilesOpened() { window.openExternalFiles() }
    }
    Connections {
        target: Tools
        function onInvalidInput(message) { window.toast("warning", message) }
    }
    Connections {
        target: Jobs
        function onFinished(title, outputs, warnings, summary) {
            const name = outputs.length > 0 ? outputs[0].split(/[\\/]/).pop() : ""
            const headline = outputs.length === 1 ? qsTr("Saved %1").arg(name)
                                                  : qsTr("Done: %n file(s) saved", "", outputs.length)
            const notes = warnings.length > 0
                ? qsTr("%n issue(s) in the input were repaired or noted.", "", warnings.length) : ""
            window.toast(warnings.length > 0 ? "warning" : "success", headline,
                         [summary, notes].filter(s => s.length > 0).join("\n"),
                         outputs.length > 0 ? outputs[0] : "", warnings.join("\n"))
        }
        function onFailed(title, message, cancelled) {
            if (cancelled)
                window.toast("info", qsTr("Cancelled"), qsTr("Nothing was changed."))
            else
                window.toast("error", qsTr("%1 failed").arg(title),
                             message + "\n" + qsTr("Your original files were not changed."))
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Theme.sidebarWidth
            color: Theme.sidebar

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingM
                spacing: Theme.spacingXxs

                RowLayout {
                    Layout.bottomMargin: Theme.spacingM
                    Layout.leftMargin: Theme.spacingXs
                    spacing: Theme.spacingS + Theme.spacingXs
                    Image {
                        source: "brand/app-icon.svg"
                        sourceSize: Qt.size(64, 64)
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                    }
                    Label {
                        text: qsTr("PDF Handler")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeL
                        font.weight: Theme.weightSemibold
                    }
                }

                Repeater {
                    model: window.tools.filter(t => t.key !== "settings")
                    delegate: ColumnLayout {
                        required property var modelData
                        required property int index
                        readonly property var previous: index > 0 ? window.tools[index - 1] : null
                        Layout.fillWidth: true
                        spacing: Theme.spacingXxs
                        NavHeading {
                            visible: modelData.group.length > 0 && (!previous || previous.group !== modelData.group)
                            text: modelData.group
                        }
                        NavItem {
                            Layout.fillWidth: true
                            text: modelData.title
                            iconName: modelData.icon
                            current: window.currentKey === modelData.key
                            onClicked: window.navigate(modelData.key)
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                NavItem {
                    Layout.fillWidth: true
                    text: qsTr("Settings")
                    iconName: "settings"
                    current: window.currentKey === "settings"
                    onClicked: window.navigate("settings")
                }
                RowLayout {
                    Layout.topMargin: Theme.spacingXs
                    spacing: Theme.spacingXxs
                    IconButton {
                        iconName: "monitor"; size: Theme.controlHeightSmall; checkable: true
                        checked: Theme.mode === "system"; tip: qsTr("Follow system theme")
                        onClicked: Theme.mode = "system"
                    }
                    IconButton {
                        iconName: "sun"; size: Theme.controlHeightSmall; checkable: true
                        checked: Theme.mode === "light"; tip: qsTr("Light theme")
                        onClicked: Theme.mode = "light"
                    }
                    IconButton {
                        iconName: "moon"; size: Theme.controlHeightSmall; checkable: true
                        checked: Theme.mode === "dark"; tip: qsTr("Dark theme")
                        onClicked: Theme.mode = "dark"
                    }
                }
            }

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.border
            }
        }

        // Pages, in the same order as `tools`. Each keeps its state while you switch away.
        StackLayout {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.currentIndex

            HomePage {
                tools: window.tools
                onToolRequested: (key) => window.navigate(key)
                onFilesDropped: (urls) => { window.pageFor("organize").addFiles(urls); window.navigate("organize") }
            }
            OrganizePage {}
            MergePage {}
            SplitPage {}
            CompressPage {}
            ProtectPage {}
            UnlockPage {}
            PropertiesPage {}
            ImagesToPdfPage { onToastRequested: (kind, title, message) => window.toast(kind, title, message) }
            PdfToImagesPage {}
            SettingsPage { onToastRequested: (kind, title, message) => window.toast(kind, title, message) }
        }
    }

    JobBar {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: Theme.sidebarWidth / 2
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spacingL
        width: Math.min(480, parent.width - Theme.sidebarWidth - 2 * Theme.spacingL)
        z: 10
    }

    ToastHost {
        id: toasts
        anchors.fill: parent
        z: 20
    }
}
