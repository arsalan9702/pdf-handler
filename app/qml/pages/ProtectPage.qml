import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import PdfHandler

ToolLayout {
    id: page

    function addFiles(urls) { docs.addUrls(urls) }

    DocumentList { id: docs }

    readonly property bool mismatch: confirm.text.length > 0 && confirm.text !== password.text
    readonly property bool ready: password.text.length > 0 && confirm.text === password.text

    title: qsTr("Protect PDF")
    subtitle: qsTr("Require a password to open the file, using AES-256 encryption.")
    files: DocumentPanel { documents: docs }

    actionText: docs.count > 1 ? qsTr("Protect %n files", "", docs.count) : qsTr("Protect")
    actionIcon: "lock"
    actionEnabled: docs.allReady && ready
    actionHint: qsTr("Keep the password safe: it cannot be recovered.")
    onAction: {
        if (docs.count === 1) {
            saveDialog.currentFile = Tools.suggestedFile(docs, 0, "-protected")
            saveDialog.open()
        } else {
            folderDialog.currentFolder = Tools.folderOf(docs, 0)
            folderDialog.open()
        }
    }

    function run(target) {
        Tools.protect(docs, password.text, ownerPassword.text, allowPrint.checked, allowCopy.checked,
                      allowModify.checked, target)
    }

    SectionTitle { text: qsTr("Password") }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingXs
        FieldLabel { text: qsTr("Password to open") }
        PasswordField { id: password; Layout.fillWidth: true }
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingXs
        FieldLabel { text: qsTr("Confirm password") }
        PasswordField { id: confirm; Layout.fillWidth: true; errorText: page.mismatch ? "x" : "" }
        HelpText { visible: page.mismatch; error: true; text: qsTr("Passwords don't match") }
    }

    SectionTitle { text: qsTr("Permissions"); Layout.topMargin: Theme.spacingS }
    AppCheckBox { id: allowPrint; Layout.fillWidth: true; text: qsTr("Allow printing"); checked: true }
    AppCheckBox { id: allowCopy; Layout.fillWidth: true; text: qsTr("Allow copying text and images"); checked: true }
    AppCheckBox { id: allowModify; Layout.fillWidth: true; text: qsTr("Allow editing and annotations"); checked: true }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.spacingXs
        FieldLabel { text: qsTr("Permissions password (optional)") }
        PasswordField { id: ownerPassword; Layout.fillWidth: true; placeholderText: qsTr("Same as above") }
        HelpText {
            Layout.fillWidth: true
            text: qsTr("Lets you lift the restrictions later. Viewers are not required to enforce permissions.")
        }
    }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: [qsTr("PDF files (*.pdf)")]
        onAccepted: page.run(selectedFile)
    }
    FolderDialog { id: folderDialog; onAccepted: page.run(selectedFolder) }
}
