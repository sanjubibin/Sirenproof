import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sirenproof.Backend

// What's happening right now: the mast ring, what is blocked, waiting changes and breaks.
Flickable {
    id: root

    required property real now
    signal startLock()
    signal lockDown()
    signal message(string text)
    signal report(var result, string success)

    readonly property var lock: (Service.status.locked || []).length ? Service.status.locked[0] : null
    readonly property real until: lock && lock.until ? Date.parse(lock.until) : 0
    readonly property real from: lock && lock.from ? Date.parse(lock.from) : 0
    readonly property real remaining: Math.max(0, until - now)
    readonly property var unlocking: lock ? (Service.status.unlocking || []).find(u => u.rule === lock.rule) : undefined
    readonly property bool wide: width >= 820
    readonly property int waitMinutes: lock && lock.wait_minutes ? lock.wait_minutes : 30

    contentHeight: layout.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    GridLayout {
        id: layout
        width: root.width
        columns: root.wide ? 2 : 1
        columnSpacing: 24
        rowSpacing: 24

        // ---- The session ----
        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            Layout.rowSpan: root.wide ? 4 : 1
            padding: 32
            spacing: 18

            MastRing {
                Layout.alignment: Qt.AlignHCenter
                progress: root.lock && root.from && root.until > root.from ? root.remaining / (root.until - root.from) : 0
                bound: root.lock !== null
                animated: Service.animations
                Accessible.role: Accessible.ProgressBar
                Accessible.name: root.lock ? `Locked, ${Theme.span(root.remaining)} left` : "Nothing is locked"

                Column {
                    spacing: 2
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.lock ? "LOCKED" : "OPEN WATER"
                        color: root.lock ? Theme.accent : Theme.muted
                        font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.lock ? Theme.span(root.remaining) : "—"
                        color: Theme.text
                        font.family: Theme.display; font.pixelSize: 44; font.weight: Font.DemiBold
                        font.features: { "tnum": 1 }
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.lock ? `until ${Theme.clock(root.until)}` : "nothing is locked"
                        color: Theme.muted; font.pixelSize: 14
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.lock ? (root.lock.name || "Locked") : "Nothing locked right now"
                color: Theme.text
                font.family: Theme.display; font.pixelSize: 28; font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                text: root.lock ? (root.lock.goal || "") : "Your scheduled rules still apply."
                visible: text.length > 0
                color: Theme.muted
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            SirenButton {
                Layout.alignment: Qt.AlignHCenter
                visible: !root.lock
                kind: "primary"
                text: "Start a lock"
                onClicked: root.startLock()
            }
            SirenButton {
                id: unlockToggle
                property bool open: false
                Layout.alignment: Qt.AlignHCenter
                visible: root.lock !== null
                text: open ? "Never mind" : "Unlock early…"
                onClicked: open = !open
            }

            // ---- Unlocking early, the way the lock was set up ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 540
                Layout.alignment: Qt.AlignHCenter
                visible: root.lock !== null && unlockToggle.open
                spacing: 12

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.edge }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    text: !root.lock ? "" : {
                        "wait": `Ask now, and the lock ends ${root.waitMinutes} minutes from now unless you change your mind.`,
                        "type": "Copy a long random text by hand. It takes a few minutes, which is the point.",
                        "friend": "Enter the code you gave to someone you trust when the lock began.",
                        "none": `This lock has no early exit. It ends at ${Theme.clock(root.until)}.`
                    }[root.lock.unlock]
                }

                // Wait
                RowLayout {
                    visible: root.lock !== null && root.lock.unlock === "wait"
                    spacing: 12
                    Text {
                        visible: root.unlocking !== undefined
                        text: root.unlocking ? `Unlocks at ${Theme.clock(Date.parse(root.unlocking.due))} · ${Theme.span(Date.parse(root.unlocking.due) - root.now)}` : ""
                        color: Theme.amber
                        font.family: Theme.display; font.pixelSize: 20; font.features: { "tnum": 1 }
                    }
                    SirenButton {
                        kind: "amber"
                        text: root.unlocking ? "Keep the lock" : `Start the ${root.waitMinutes}-minute wait`
                        onClicked: root.report(root.unlocking ? Service.cancelUnlock(root.lock.rule) : Service.requestUnlock(root.lock.rule),
                                               root.unlocking ? "Good call. The lock stays." : "The wait has started. You can still change your mind.")
                    }
                }

                // Type it out
                TypeChallenge {
                    Layout.fillWidth: true
                    visible: root.lock !== null && root.lock.unlock === "type"
                    lock: root.lock
                    onMessage: text => root.message(text)
                    onReport: (result, success) => root.report(result, success)
                    onUnlocked: unlockToggle.open = false
                }

                // A friend's code
                FriendCodeEntry {
                    Layout.fillWidth: true
                    visible: root.lock !== null && root.lock.unlock === "friend"
                    lock: root.lock
                    now: root.now
                    onReport: (result, success) => root.report(result, success)
                    onUnlocked: unlockToggle.open = false
                }
            }
        }

        // ---- Blocking now ----
        Card {
            Layout.fillWidth: true
            Layout.preferredWidth: root.wide ? 340 : -1
            Layout.maximumWidth: root.wide ? 380 : 100000
            Layout.alignment: Qt.AlignTop

            Text { text: "BLOCKING NOW"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: {
                        const entries = (Service.status.web || []).concat(Service.status.apps || [])
                        return [...new Set(entries.filter(e => e.action !== "allow").map(e => e.target))]
                    }
                    delegate: Rectangle {
                        required property string modelData
                        width: chipText.implicitWidth + 24
                        height: 32
                        radius: 16
                        color: Theme.accentSoft
                        border.color: Theme.edgeStrong
                        Text { id: chipText; anchors.centerIn: parent; text: parent.modelData; color: Theme.text; font.pixelSize: 14 }
                    }
                }
            }
            Text {
                visible: !((Service.status.web || []).length || (Service.status.apps || []).length)
                text: "Nothing right now."
                color: Theme.muted
            }
        }

        // ---- Waiting changes ----
        Card {
            Layout.fillWidth: true
            Layout.preferredWidth: root.wide ? 340 : -1
            Layout.maximumWidth: root.wide ? 380 : 100000
            Layout.alignment: Qt.AlignTop
            visible: (Service.status.pending || []).length > 0
            border.color: Theme.amberEdge

            Text { text: "WAITING TO APPLY"; color: Theme.amber; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
            Repeater {
                model: Service.status.pending || []
                delegate: ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: Theme.text
                        text: {
                            const [kind, id] = modelData.what.split(" ")
                            if (kind === "remove") return `Remove the rule “${Theme.ruleName(Service.rules, id)}”`
                            if (kind === "change") return `Loosen “${Theme.ruleName(Service.rules, id)}”`
                            return "Loosen the settings"
                        }
                    }
                    Text {
                        text: `Applies at ${Theme.clock(Date.parse(modelData.applies_at))} · ${Theme.span(Date.parse(modelData.applies_at) - root.now)} left`
                        color: Theme.muted; font.pixelSize: 14; font.features: { "tnum": 1 }
                    }
                    SirenButton {
                        kind: "amber"
                        text: "Cancel this change"
                        onClicked: root.report(Service.cancelPending(modelData.id), "Cancelled. Nothing loosens.")
                    }
                }
            }
        }

        // ---- Breaks ----
        Card {
            id: breaksCard
            Layout.fillWidth: true
            Layout.preferredWidth: root.wide ? 340 : -1
            Layout.maximumWidth: root.wide ? 380 : 100000
            Layout.alignment: Qt.AlignTop
            readonly property var breaks: Service.status.breaks || { used_today: 0, per_day: 0 }
            readonly property bool lockedOut: root.lock !== null && !root.lock.breaks_allowed

            Text { text: "BREAKS TODAY"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
            Text {
                text: `${breaksCard.breaks.used_today} of ${breaksCard.breaks.per_day} used`
                color: Theme.text; font.family: Theme.display; font.pixelSize: 22; font.features: { "tnum": 1 }
            }
            SirenButton {
                text: "Take a 5-minute break"
                enabled: !breaksCard.lockedOut && breaksCard.breaks.used_today < breaksCard.breaks.per_day
                onClicked: root.report(Service.startBreak(), "Five minutes. It ends on its own.")
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: breaksCard.lockedOut ? "This lock doesn't pause for breaks." : "Breaks end on their own."
                color: Theme.muted; font.pixelSize: 13
            }
        }

        // ---- Offered once there's a lock to protect ----
        Card {
            id: lockDownCard
            property var accounts: Accounts.read()
            Layout.fillWidth: true
            Layout.preferredWidth: root.wide ? 340 : -1
            Layout.maximumWidth: root.wide ? 380 : 100000
            Layout.alignment: Qt.AlignTop
            visible: root.lock !== null && !accounts.error && accounts.admin

            Timer {
                interval: 60000
                running: root.visible
                repeat: true
                onTriggered: lockDownCard.accounts = Accounts.read()
            }

            Text { text: "HARDER TO SWITCH OFF"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: "Your account is an administrator, so it could stop Sirenproof. Lock down this PC to make that need someone else's password."
                color: Theme.text; font.pixelSize: 14
            }
            SirenButton {
                text: "Lock down this PC"
                onClicked: root.lockDown()
            }
        }
    }
}
