pragma Singleton
import QtQuick

// The deep-water HUD (plan §3.7, as approved in the prototype). Colour always means something:
// sea glass for whatever is active, amber for anything paused or waiting.
QtObject {
    readonly property color ground: "#050b18"
    readonly property color groundTop: "#0b1d3a"
    readonly property color surface: Qt.rgba(10 / 255, 26 / 255, 51 / 255, 0.78)
    readonly property color sunken: Qt.rgba(4 / 255, 10 / 255, 22 / 255, 0.55)
    readonly property color edge: Qt.rgba(92 / 255, 225 / 255, 230 / 255, 0.16)
    readonly property color edgeStrong: Qt.rgba(92 / 255, 225 / 255, 230 / 255, 0.32)
    readonly property color text: "#e6f1ff"
    readonly property color muted: "#9fb6d6"
    readonly property color accent: "#5ce1e6"
    readonly property color accentSoft: Qt.rgba(92 / 255, 225 / 255, 230 / 255, 0.10)
    readonly property color inkOnAccent: "#04101f"
    readonly property color amber: "#f5b14c"
    readonly property color amberEdge: Qt.rgba(245 / 255, 177 / 255, 76 / 255, 0.55)
    readonly property color good: "#8ee3a1"
    readonly property color bad: "#ff9d9d"

    // Windows 11's variable Segoe where it exists, plain Segoe UI otherwise.
    readonly property var installedFonts: Qt.fontFamilies()
    function pick(wanted) {
        for (const family of wanted) {
            if (installedFonts.indexOf(family) >= 0) return family
        }
        return wanted[wanted.length - 1]
    }
    readonly property string display: pick(["Segoe UI Variable Display", "Segoe UI Variable", "Segoe UI"])
    readonly property string body: pick(["Segoe UI Variable Text", "Segoe UI Variable", "Segoe UI"])
    readonly property int radius: 20

    // Rule ids as people read them: the rule's name where it has one.
    function ruleName(rules, id) {
        const rule = (rules || []).find(r => r.id === id)
        return rule && rule.name ? rule.name : id
    }

    function clock(ms) { return Qt.formatTime(new Date(ms), "hh:mm") }

    function span(ms) {
        const s = Math.max(0, Math.round(ms / 1000))
        const h = Math.floor(s / 3600)
        const m = Math.floor((s % 3600) / 60)
        const pad = n => String(n).padStart(2, "0")
        return h > 0 ? `${h}:${pad(m)}:${pad(s % 60)}` : `${m}:${pad(s % 60)}`
    }
}
