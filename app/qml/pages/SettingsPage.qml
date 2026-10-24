import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PdfHandler

Item {
    id: page

    signal toastRequested(string kind, string title, string message)

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: Math.min(parent.width - 2 * Theme.spacingL, 640)
            x: Theme.spacingL
            spacing: Theme.spacingL

            PageHeader {
                Layout.fillWidth: true
                Layout.topMargin: Theme.spacingL
                title: qsTr("Settings")
            }

            Card {
                Layout.fillWidth: true
                implicitHeight: appearance.implicitHeight + 2 * Theme.spacingM
                ColumnLayout {
                    id: appearance
                    anchors.fill: parent
                    anchors.margins: Theme.spacingM
                    spacing: Theme.spacingS
                    SectionTitle { text: qsTr("Appearance") }
                    SegmentedControl {
                        options: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                        currentIndex: ["system", "light", "dark"].indexOf(Theme.mode)
                        onActivated: (index) => Theme.mode = ["system", "light", "dark"][index]
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                visible: App.canInstallLauncher
                implicitHeight: launcher.implicitHeight + 2 * Theme.spacingM
                ColumnLayout {
                    id: launcher
                    anchors.fill: parent
                    anchors.margins: Theme.spacingM
                    spacing: Theme.spacingS
                    SectionTitle { text: qsTr("Desktop integration") }
                    HelpText {
                        Layout.fillWidth: true
                        text: App.launcherInstalled
                              ? qsTr("PDF Handler is in your app launcher and in \"Open with\" for PDF files. The pdfh command is available in new terminals.")
                              : qsTr("Add PDF Handler to your app launcher and to \"Open with\" for PDF files, and install the pdfh command for terminals.")
                    }
                    AppCheckBox {
                        id: makeDefault
                        visible: !App.launcherInstalled
                        text: qsTr("Also make it the default app for PDF files")
                    }
                    RowLayout {
                        spacing: Theme.spacingS
                        AppButton {
                            kind: App.launcherInstalled ? "secondary" : "primary"
                            iconName: "download"
                            text: App.launcherInstalled ? qsTr("Reinstall") : qsTr("Add to launcher")
                            onClicked: {
                                const error = App.installLauncher(makeDefault.checked)
                                if (error.length > 0)
                                    page.toastRequested("error", qsTr("Could not add to launcher"), error)
                                else
                                    page.toastRequested("success", qsTr("Added to your launcher"),
                                                        qsTr("Find PDF Handler in your app menu."))
                            }
                        }
                        AppButton {
                            visible: App.launcherInstalled
                            kind: "ghost"
                            text: qsTr("Remove")
                            onClicked: App.uninstallLauncher()
                        }
                    }
                }
            }

            Card {
                Layout.fillWidth: true
                implicitHeight: about.implicitHeight + 2 * Theme.spacingM
                ColumnLayout {
                    id: about
                    anchors.fill: parent
                    anchors.margins: Theme.spacingM
                    spacing: Theme.spacingS
                    SectionTitle { text: qsTr("About") }
                    Label {
                        text: qsTr("PDF Handler %1").arg(App.version)
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeM
                    }
                    HelpText {
                        Layout.fillWidth: true
                        text: qsTr("Built with Qt (LGPL-3.0), QPDF (Apache-2.0), PDFium via Qt PDF (BSD), the Inter typeface (OFL-1.1) and Lucide icons (ISC).")
                    }
                }
            }
        }
    }
}
