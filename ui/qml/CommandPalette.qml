import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sirenproof.Backend

// The command palette (Ctrl+K). Type what you want, in the command line's words ("lock youtube 2h",
// "block reddit.com 25m", "check twitch.tv"), see exactly what it will do, press Enter. A lock opens the
// lock screen filled in, so it's still held to commit. Suggestions fill the field rather than run.
Popup {
    id: root

    signal run(var command)

    property int current: 0
    property string verdict: ""
    property color verdictTone: Theme.muted

    readonly property var parsed: Service.parseCommand(field.text)
    readonly property var suggestions: [
        { label: "Lock YouTube for 2 hours", command: "lock youtube 2h" },
        { label: "Block reddit.com for 25 minutes", command: "block reddit.com 25m" },
        { label: "Take a break", command: "break" },
        { label: "Test an address against your rules", command: "check youtube.com/shorts" },
        { label: "Go to your rules", command: "rules" },
        { label: "End a lock early", command: "unlock" },
        { label: "Lock down this PC", command: "lockdown" }
    ]
    readonly property var items: {
        const typed = field.text.trim().toLowerCase()
        const verb = typed.split(/\s+/)[0]
        const list = []
        if (!parsed.error) list.push({ label: describe(parsed), hint: "Enter", command: field.text, ready: true })
        else if (parsed.known) list.push({ label: sentence(parsed.error), hint: "", command: "", problem: true })
        const understood = !parsed.error || parsed.known
        // A suggestion matches when one of its words starts with what's typed ("b" finds block and
        // break, not YouTube), or, once a command is understood, when it's the same command.
        const matches = s => {
            if (!typed) return true
            if (understood) return s.command.startsWith(verb)
            const label = s.label.toLowerCase()
            return label.startsWith(typed) || `${label} ${s.command}`.split(/[\s./]+/).some(w => w.startsWith(typed))
        }
        for (const s of suggestions) {
            if (s.command !== typed && matches(s)) list.push({ label: s.label, hint: s.command, command: s.command })
        }
        return list
    }

    function sentence(text) {
        const t = text.charAt(0).toUpperCase() + text.slice(1)
        return /[.!?]$/.test(t) ? t : t + "."
    }
    function lengthText(m) {
        if (m < 60) return `${m} minute${m === 1 ? "" : "s"}`
        if (m % 60 === 0) return `${m / 60} hour${m === 60 ? "" : "s"}`
        return `${Math.floor(m / 60)} h ${m % 60} min`
    }
    function joined(names) {
        if (names.length > 4) return `${names.slice(0, 3).join(", ")} and ${names.length - 3} more`
        return names.length > 1 ? `${names.slice(0, -1).join(", ")} and ${names[names.length - 1]}` : names[0] || ""
    }
    function describe(c) {
        switch (c.kind) {
        case "block": {
            const what = joined(c.all_targets)
            const verb = c.strength === "nudge" ? "Nudge before" : "Block"
            return `${verb} ${what} ${c.minutes ? "for " + lengthText(c.minutes) : "until you remove it"}. Applies at once.`
        }
        case "lock": {
            const labels = Service.readyLists.filter(l => c.lists.includes(l.id)).map(l => l.label)
            const what = joined(labels.concat(c.targets))
            return `Set up a lock${what ? " on " + what : ""} for ${lengthText(c.minutes)}. You'll still hold to commit.`
        }
        case "break": return "Take a break from the blocks that allow one."
        case "check": return verdict || `Checking ${c.subject}…`
        default: return { now: "Go to Now", rules: "Go to your rules", lock: "Go to Start a lock", lockdown: "Go to Lock down this PC" }[c.screen]
        }
    }

    function openWith(text) {
        field.text = text
        open()
    }

    function choose(item) {
        if (!item || item.problem) return
        if (!item.ready) {
            field.text = item.command
            field.cursorPosition = field.text.length
            return
        }
        const command = parsed
        close()
        run(command)
    }

    modal: true
    focus: true
    padding: 0
    x: Math.round((parent.width - width) / 2)
    y: Math.round(parent.height * 0.12)
    width: Math.min(620, parent.width - 32)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: { field.forceActiveFocus(); field.cursorPosition = field.text.length }
    onClosed: { field.text = ""; current = 0 }
    Overlay.modal: Rectangle { color: Qt.rgba(2 / 255, 6 / 255, 14 / 255, 0.72) }

    enter: Transition {
        enabled: Service.animations
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 }
        NumberAnimation { property: "y"; from: root.parent.height * 0.12 + 10; to: root.parent.height * 0.12; duration: 160; easing.type: Easing.OutCubic }
    }

    background: Rectangle {
        radius: Theme.radius
        color: "#0a1a33"
        border.color: Qt.rgba(0.36, 0.88, 0.9, 0.35)
    }

    contentItem: ColumnLayout {
        spacing: 0

        TextField {
            id: field
            Layout.fillWidth: true
            implicitHeight: 58
            leftPadding: 20
            rightPadding: 20
            placeholderText: "Try: lock youtube 2h"
            placeholderTextColor: Theme.muted
            color: Theme.text
            font.pixelSize: 17
            Accessible.name: "Command"
            background: Rectangle {
                color: "transparent"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Qt.rgba(0.36, 0.88, 0.9, 0.18) }
            }
            onTextChanged: { root.current = 0; root.verdict = ""; checkDelay.restart() }
            Keys.onDownPressed: root.current = Math.min(root.current + 1, root.items.length - 1)
            Keys.onUpPressed: root.current = Math.max(root.current - 1, 0)
            Keys.onTabPressed: {
                const item = root.items[root.current]
                if (item && !item.ready && !item.problem) root.choose(item)
            }
            onAccepted: root.choose(root.items[root.current])
        }

        // Asks the service about a "check" while you type, a moment after you stop.
        Timer {
            id: checkDelay
            interval: 200
            onTriggered: {
                if (root.parsed.kind !== "check") return
                const v = Service.check(root.parsed.subject)
                if (v.error) { root.verdict = root.sentence(v.error); root.verdictTone = Theme.muted }
                else if (v.action === "allow") { root.verdict = `${root.parsed.subject}: allowed right now.`; root.verdictTone = Theme.good }
                else {
                    root.verdict = `${root.parsed.subject}: ${v.action === "nudge" ? "a nudge" : "blocked"} by “${Theme.ruleName(Service.rules, v.rule)}”.`
                    root.verdictTone = v.action === "nudge" ? Theme.amber : Theme.bad
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 2

            Repeater {
                model: root.items
                delegate: Button {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property bool current: index === root.current
                    Layout.fillWidth: true
                    implicitHeight: 48
                    leftPadding: 14
                    rightPadding: 14
                    hoverEnabled: true
                    focusPolicy: Qt.NoFocus
                    Accessible.name: modelData.label
                    onHoveredChanged: if (hovered) root.current = index
                    onClicked: root.choose(modelData)
                    contentItem: RowLayout {
                        spacing: 16
                        Text {
                            Layout.fillWidth: true
                            text: row.modelData.label
                            color: row.modelData.problem ? Theme.amber
                                 : row.modelData.ready && root.parsed.kind === "check" && root.verdict ? root.verdictTone
                                 : Theme.text
                            elide: Text.ElideRight
                        }
                        Text {
                            text: row.modelData.hint
                            color: Theme.muted
                            font.pixelSize: 13
                        }
                    }
                    background: Rectangle {
                        radius: 12
                        color: row.current && !row.modelData.problem ? Theme.accentSoft : "transparent"
                    }
                }
            }

            Text {
                Layout.margins: 14
                visible: root.items.length === 0
                text: "No command matches. Try lock, block, nudge, break, check, rules or lockdown."
                color: Theme.muted
            }
        }
    }
}
