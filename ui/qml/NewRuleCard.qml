import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sirenproof.Backend

// A new rule in one card: pick ready-made lists or type sites, pages and apps; Nudge or Block; always or
// painted hours. Nothing reaches the service until Create, so painting here is free. Once the rule exists,
// adding hours applies at once and clearing them waits out the rule's delay, like any loosening.
Card {
    id: root

    required property real now
    signal created(string rule)
    signal cancelled()
    signal report(var result, string success)

    readonly property var lists: Service.readyLists
    readonly property var workHours: ({ weekly: [{ days: ["mon", "tue", "wed", "thu", "fri"], from: "09:00", to: "17:00" }] })
    property var sets: ({})
    property var extra: []
    property string strength: "block"
    property bool always: false
    property var when: workHours
    readonly property var targets: lists.filter(l => sets[l.id]).reduce((all, l) => all.concat(l.targets), [])
                                        .concat(extra).filter((t, i, all) => all.indexOf(t) === i)

    spacing: 22

    function reset() {
        sets = {}
        extra = []
        strength = "block"
        always = false
        when = workHours
        name.clear()
        newTarget.clear()
    }

    function addTyped() {
        const typed = newTarget.text.split(/[\s,]+/).map(t => t.trim()).filter(t => t.length)
        if (!typed.length) return
        extra = extra.concat(typed.filter(t => !extra.includes(t)))
        newTarget.clear()
    }

    function create() {
        addTyped()
        const result = Service.createRule(targets, always ? "always" : when, strength, name.text)
        if (result.outcome === "applied") {
            created(result.rule)
            reset()
        }
        report(result, strength === "nudge" ? "Rule added. A nudge now comes first." : "Rule added. It applies at once.")
    }

    Text { text: "NEW RULE"; color: Theme.accent; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }

    TextField {
        id: name
        Layout.fillWidth: true
        placeholderText: "Name (optional), e.g. Work hours"
        color: Theme.text
        Accessible.name: "Rule name"
        background: Rectangle { radius: 12; color: Theme.sunken; border.color: name.activeFocus ? Theme.accent : Theme.edgeStrong; implicitHeight: 44 }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        Text { text: "WHAT TO BLOCK"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
        GridLayout {
            Layout.fillWidth: true
            columns: width >= 560 ? root.lists.length : 2
            columnSpacing: 10
            rowSpacing: 10
            Repeater {
                model: root.lists
                delegate: Choice {
                    required property var modelData
                    text: modelData.label
                    detail: modelData.detail
                    chosen: !!root.sets[modelData.id]
                    onClicked: {
                        const next = Object.assign({}, root.sets)
                        next[modelData.id] = !next[modelData.id]
                        root.sets = next
                    }
                }
            }
        }
        Flow {
            Layout.fillWidth: true
            visible: root.extra.length > 0
            spacing: 8
            Repeater {
                model: root.extra
                delegate: TargetChip {
                    required property string modelData
                    target: modelData
                    removeName: `Leave out ${modelData}`
                    onRemoved: root.extra = root.extra.filter(t => t !== modelData)
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            TextField {
                id: newTarget
                Layout.fillWidth: true
                placeholderText: "More: reddit.com, youtube.com/shorts or app:steam.exe"
                color: Theme.text
                Accessible.name: "Add a site, page or app"
                background: Rectangle { radius: 12; color: Theme.sunken; border.color: newTarget.activeFocus ? Theme.accent : Theme.edgeStrong; implicitHeight: 44 }
                onAccepted: root.addTyped()
            }
            SirenButton {
                text: "Add"
                enabled: newTarget.text.trim().length > 0
                onClicked: root.addTyped()
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        Text { text: "HOW STRONGLY"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Choice {
                text: "Nudge"
                detail: "A pause with your goal, then you may go on"
                chosen: root.strength === "nudge"
                onClicked: root.strength = "nudge"
            }
            Choice {
                text: "Block"
                detail: "Blocked; loosening waits 10 minutes"
                chosen: root.strength === "block"
                onClicked: root.strength = "block"
            }
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: "For a lock that can't be loosened at all, use Start a lock."
            color: Theme.muted; font.pixelSize: 13
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10
        Text { text: root.always ? "WHEN" : "WHEN · PAINT THE HOURS"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Choice {
                text: "On a schedule"
                detail: "Only in the hours you paint"
                chosen: !root.always
                onClicked: root.always = false
            }
            Choice {
                text: "Always"
                detail: "Every hour of every day"
                chosen: root.always
                onClicked: root.always = true
            }
        }
        ScheduleGrid {
            id: painter
            Layout.fillWidth: true
            visible: !root.always
            when: root.when
            now: root.now
            offsetMinutes: Service.status.local_offset_minutes || 0
            onPainted: (hours, on) => {
                const next = Service.paintHours(root.when, hours, on)
                if (!next) {
                    root.report({ error: "A schedule needs at least one hour. Choose Always to block every hour" }, "")
                    return
                }
                if (next === "always") root.always = true
                else root.when = next
            }
        }
        Text {
            Layout.fillWidth: true
            visible: !root.always
            text: painter.caption || "Mon–Fri 09:00–17:00 to start with. Drag to add hours, or start on a lit hour to clear them. Nothing is saved until you press Create."
            color: painter.caption ? Theme.text : Theme.muted
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }
    }

    RowLayout {
        spacing: 10
        SirenButton {
            kind: "primary"
            text: root.targets.length ? `Create (${root.targets.length} ${root.targets.length === 1 ? "target" : "targets"})` : "Create"
            enabled: root.targets.length > 0 || newTarget.text.trim().length > 0
            onClicked: root.create()
        }
        SirenButton {
            text: "Cancel"
            onClicked: { root.reset(); root.cancelled() }
        }
    }
}
