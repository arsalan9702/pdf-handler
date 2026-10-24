pragma Singleton

import QtQuick

// Single source of truth for colors, spacing, radii, type and motion.
QtObject {
    // "system" | "light" | "dark"; persisted by Main.qml.
    property string mode: "system"
    readonly property bool dark: mode === "dark"
        || (mode === "system" && Qt.styleHints.colorScheme === Qt.ColorScheme.Dark)

    // Surfaces
    readonly property color background: dark ? "#0f1115" : "#f5f6f8"
    readonly property color sidebar: dark ? "#14161b" : "#eef0f4"
    readonly property color surface: dark ? "#1a1d23" : "#ffffff"
    readonly property color surfaceAlt: dark ? "#21252c" : "#f8f9fb"
    readonly property color surfaceHover: dark ? "#272b33" : "#f1f3f6"
    readonly property color surfacePressed: dark ? "#2e333c" : "#e8ebf0"
    readonly property color border: dark ? "#2b3038" : "#e3e6eb"
    readonly property color borderStrong: dark ? "#3a404a" : "#cfd4dc"
    readonly property color inputBackground: dark ? "#14171c" : "#ffffff"
    readonly property color shadow: dark ? "#66000000" : "#1a0f172a"
    readonly property color scrim: dark ? "#99000000" : "#66101418"

    // Text
    readonly property color textPrimary: dark ? "#eef0f3" : "#14171c"
    readonly property color textSecondary: dark ? "#a3a9b4" : "#5b6270"
    readonly property color textTertiary: dark ? "#737a86" : "#8a909b"
    readonly property color textOnAccent: "#ffffff"

    // Accent and status
    readonly property color accent: dark ? "#5b8cff" : "#2f6bf2"
    readonly property color accentHover: dark ? "#729cff" : "#2459d6"
    readonly property color accentPressed: dark ? "#4a78e6" : "#1d4cbc"
    readonly property color accentSoft: dark ? "#1f2d4d" : "#e7eefe"
    readonly property color selection: dark ? "#5b8cff" : "#2f6bf2"
    readonly property color danger: dark ? "#f47174" : "#d93a3d"
    readonly property color dangerSoft: dark ? "#3a1f22" : "#fdecec"
    readonly property color warning: dark ? "#f0b65a" : "#b7791f"
    readonly property color warningSoft: dark ? "#3a2f1c" : "#fdf4e3"
    readonly property color success: dark ? "#4cc38a" : "#1f9d5c"
    readonly property color successSoft: dark ? "#183326" : "#e6f6ee"

    // One colour per source document in the organizer.
    readonly property var docColors: ["#2f6bf2", "#e5534b", "#2da44e", "#bf8700", "#8250df",
                                      "#1b9aaa", "#d4558a", "#6e7781"]

    // Spacing (8px grid)
    readonly property int unit: 8
    readonly property int spacingXxs: 2
    readonly property int spacingXs: unit / 2
    readonly property int spacingS: unit
    readonly property int spacingM: unit * 2
    readonly property int spacingL: unit * 3
    readonly property int spacingXl: unit * 4
    readonly property int spacingXxl: unit * 6

    // Sizes
    readonly property int controlHeight: 36
    readonly property int controlHeightSmall: 28
    readonly property int sidebarWidth: 240
    readonly property int iconSize: 18
    readonly property int iconSizeSmall: 16
    readonly property int iconSizeLarge: 24
    readonly property int panelWidth: 340
    readonly property int contentMaxWidth: 1080

    // Radii
    readonly property int radiusCard: 8
    readonly property int radiusControl: 6
    readonly property int radiusPill: 999

    // Type
    readonly property string fontFamily: "Inter"
    readonly property int fontSizeXs: 11
    readonly property int fontSizeS: 12
    readonly property int fontSizeM: 14
    readonly property int fontSizeL: 16
    readonly property int fontSizeXl: 20
    readonly property int fontSizeXxl: 28
    readonly property int weightRegular: Font.Normal
    readonly property int weightMedium: Font.Medium
    readonly property int weightSemibold: Font.DemiBold

    // Motion
    readonly property int durationFast: 150
    readonly property int durationNormal: 200
    readonly property int toastDuration: 5000
}
