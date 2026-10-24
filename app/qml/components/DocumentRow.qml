import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

// One file in a DocumentList: name, size and pages, plus inline unlock / error / range field.
Rectangle {
    id: row

    required property int index
    required property string name
    required property string folder
    required property int pageCount
    required property bool encrypted
    required property int status
    required property string error
    required property var warnings
    required property real size
    required property string range

    required property DocumentList documents
    property bool showRange: false
    property bool reorderable: false

    implicitHeight: content.implicitHeight + 2 * Theme.spacingS + Theme.spacingXs
    radius: Theme.radiusControl
    color: hover.hovered ? Theme.surfaceHover : Theme.surface
    border.width: 1
    border.color: status === DocumentList.Failed ? Theme.danger
                : status === DocumentList.NeedsPassword || status === DocumentList.WrongPassword
                  ? Theme.warning : Theme.border
    Behavior on color { ColorAnimation { duration: Theme.durationFast } }

    HoverHandler { id: hover }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Theme.spacingS + Theme.spacingXxs
        spacing: Theme.spacingS

        RowLayout {
            spacing: Theme.spacingS + Theme.spacingXs

            Rectangle {
                width: 36
                height: 36
                radius: Theme.radiusControl
                color: row.status === DocumentList.Failed ? Theme.dangerSoft : Theme.accentSoft
                AppIcon {
                    anchors.centerIn: parent
                    name: row.status === DocumentList.Failed ? "triangle-alert"
                        : row.encrypted || row.status === DocumentList.NeedsPassword
                          || row.status === DocumentList.WrongPassword ? "lock" : "file-text"
                    color: row.status === DocumentList.Failed ? Theme.danger : Theme.accent
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingXxs
                Label {
                    Layout.fillWidth: true
                    text: row.name
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeM
                    font.weight: Theme.weightMedium
                    elide: Text.ElideMiddle
                }
                Label {
                    Layout.fillWidth: true
                    color: row.status === DocumentList.Failed ? Theme.danger : Theme.textSecondary
                    font.pixelSize: Theme.fontSizeS
                    elide: Text.ElideRight
                    text: {
                        switch (row.status) {
                        case DocumentList.Loading: return qsTr("Opening…")
                        case DocumentList.NeedsPassword: return qsTr("Password protected")
                        case DocumentList.WrongPassword: return qsTr("Incorrect password, try again")
                        case DocumentList.Failed: return row.error
                        default:
                            return qsTr("%n page(s)", "", row.pageCount) + "  ·  " + Tools.formatBytes(row.size)
                                + (row.warnings.length > 0 ? "  ·  " + qsTr("repaired") : "")
                        }
                    }
                }
            }

            BusyIndicator {
                visible: row.status === DocumentList.Loading
                running: visible
                implicitWidth: 20
                implicitHeight: 20
            }
            AppIcon {
                visible: row.warnings.length > 0
                name: "info"
                color: Theme.warning
                HoverHandler { id: warnHover }
                ToolTip.visible: warnHover.hovered
                ToolTip.text: qsTr("This file had problems that were repaired when it was opened:")
                    + "\n• " + row.warnings.slice(0, 5).join("\n• ")
            }
            IconButton {
                visible: row.reorderable
                iconName: "arrow-up"
                tip: qsTr("Move up")
                size: Theme.controlHeightSmall
                enabled: row.index > 0
                onClicked: row.documents.move(row.index, row.index - 1)
            }
            IconButton {
                visible: row.reorderable
                iconName: "arrow-down"
                tip: qsTr("Move down")
                size: Theme.controlHeightSmall
                enabled: row.index < row.documents.count - 1
                onClicked: row.documents.move(row.index, row.index + 1)
            }
            IconButton {
                iconName: "x"
                tip: qsTr("Remove")
                size: Theme.controlHeightSmall
                danger: true
                onClicked: row.documents.remove(row.index)
            }
        }

        // Unlock inline instead of a modal.
        RowLayout {
            visible: row.status === DocumentList.NeedsPassword || row.status === DocumentList.WrongPassword
            spacing: Theme.spacingS
            AppTextField {
                id: password
                Layout.fillWidth: true
                placeholderText: qsTr("Password")
                echoMode: TextInput.Password
                errorText: row.status === DocumentList.WrongPassword ? "wrong" : ""
                onAccepted: row.documents.unlock(row.index, text)
            }
            AppButton {
                text: qsTr("Unlock")
                iconName: "key-round"
                enabled: password.text.length > 0
                onClicked: row.documents.unlock(row.index, password.text)
            }
        }

        ColumnLayout {
            visible: row.showRange && row.status === DocumentList.Ready
            spacing: Theme.spacingXs
            AppTextField {
                id: rangeField
                Layout.fillWidth: true
                placeholderText: qsTr("All pages — or e.g. 1-3, 7, 10-")
                text: row.range
                errorText: row.documents.rangeError(row.index, text)
                onTextEdited: row.documents.setRange(row.index, text)
            }
            HelpText {
                visible: rangeField.hasError
                error: true
                text: rangeField.errorText
            }
        }
    }
}
