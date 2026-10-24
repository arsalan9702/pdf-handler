pragma Singleton

import QtQuick

// Single source of truth for colors, spacing, radii, type and motion.
QtObject {
    id: theme

    // "system" | "light" | "dark"; wired to settings in M3.
    property string mode: "system"
    readonly property bool dark: mode === "dark"
        || (mode === "system" && Qt.styleHints.colorScheme === Qt.ColorScheme.Dark)

    // Colors
    readonly property color background: dark ? "#111214" : "#f7f7f8"
    readonly property color surface: dark ? "#1a1b1e" : "#ffffff"
    readonly property color surfaceRaised: dark ? "#232428" : "#ffffff"
    readonly property color border: dark ? "#2e3036" : "#e4e4e7"
    readonly property color textPrimary: dark ? "#f4f4f5" : "#18181b"
    readonly property color textSecondary: dark ? "#a1a1aa" : "#52525b"
    readonly property color accent: dark ? "#60a5fa" : "#2563eb"
    readonly property color accentText: "#ffffff"
    readonly property color danger: dark ? "#f87171" : "#dc2626"
    readonly property color success: dark ? "#4ade80" : "#16a34a"

    // Spacing (8px grid)
    readonly property int unit: 8
    readonly property int spacingXs: unit / 2
    readonly property int spacingS: unit
    readonly property int spacingM: unit * 2
    readonly property int spacingL: unit * 3
    readonly property int spacingXl: unit * 4

    // Radii
    readonly property int radiusCard: 8
    readonly property int radiusControl: 6

    // Type
    readonly property string fontFamily: "Inter"
    readonly property int fontSizeS: 12
    readonly property int fontSizeM: 14
    readonly property int fontSizeL: 18
    readonly property int fontSizeXl: 24

    // Motion
    readonly property int durationFast: 150
    readonly property int durationNormal: 200
}
