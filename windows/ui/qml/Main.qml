import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sirenproof.Backend

ApplicationWindow {
    id: window

    required property string screen
    // From the command line: text to open the command palette with, or a command to run at start.
    property string paletteText: ""
    property string runText: ""
    // Trusted time from the service, ticking locally between refreshes.
    property real skew: 0
    property real now: Date.now()

    width: 1180
    height: 780
    minimumWidth: 880
    minimumHeight: 600
    visible: true
    title: "Sirenproof"
    color: Theme.ground
    font.family: Theme.body
    font.pixelSize: 15

    function say(message) {
        // The service writes times in UTC (2026-11-02T18:30:00Z); show them on this PC's clock.
        let text = String(message).replace(/\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ/g, iso => {
            const ms = Date.parse(iso)
            return ms - Date.now() > 20 * 3600e3 ? Qt.formatDateTime(new Date(ms), "ddd hh:mm") : Theme.clock(ms)
        })
        text = text.charAt(0).toUpperCase() + text.slice(1)
        toastText.text = /[.!?]$/.test(text) ? text : text + "."
        toast.shown = true
        toastTimer.restart()
    }

    // Does what the command palette understood.
    function runCommand(c) {
        switch (c.kind) {
        case "block": {
            const what = c.all_targets.length === 1 ? c.all_targets[0] : `${c.all_targets.length} sites and apps`
            const until = c.minutes ? ` until ${Theme.clock(window.now + c.minutes * 60000)}` : ""
            report(Service.addRule(c.all_targets, c.minutes, c.strength),
                   c.strength === "nudge" ? `A nudge now comes before ${what}${until}.` : `${what} is blocked${until}.`)
            break
        }
        case "lock":
            window.screen = "lock"
            lockScreen.prefill(c.lists, c.targets, c.minutes, c.unlock)
            break
        case "break":
            report(Service.startBreak(), "Break started. Blocks that allow breaks are off until it ends.")
            break
        case "check":
            window.screen = "rules"
            rulesScreen.test(c.subject)
            break
        default:
            window.screen = c.screen
        }
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: commandPalette.open()
    }

    CommandPalette {
        id: commandPalette
        onRun: command => window.runCommand(command)
    }

    Component.onCompleted: {
        if (paletteText) commandPalette.openWith(paletteText)
        if (runText) {
            const command = Service.parseCommand(runText)
            if (command.error) say(command.error)
            else runCommand(command)
        }
    }

    // Turns a service result into a short message, or `success` if all went well.
    function report(result, success) {
        if (result.error) say(result.error)
        else if (result.outcome === "refused" || result.outcome === "pending") say(result.message)
        else if (result.ok === false) say(result.message)
        else if (success) say(success)
    }

    Connections {
        target: Service
        function onChanged() {
            if (Service.status.now) window.skew = Date.parse(Service.status.now) - Date.now()
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: window.now = Date.now() + window.skew
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.groundTop }
            GradientStop { position: 0.55; color: Theme.ground }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Sidebar ----
        Item {
            Layout.fillHeight: true
            Layout.preferredWidth: 236

            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.edge }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                anchors.topMargin: 28
                spacing: 6

                RowLayout {
                    Layout.leftMargin: 10
                    Layout.bottomMargin: 22
                    spacing: 10
                    Rectangle {
                        width: 26; height: 26; radius: 13
                        color: "transparent"; border.width: 3; border.color: Theme.accent
                        Rectangle { anchors.centerIn: parent; width: 3; height: 18; radius: 1.5; color: Theme.accent }
                    }
                    Text {
                        text: "Sirenproof"
                        color: Theme.text
                        font.family: Theme.display; font.pixelSize: 20; font.weight: Font.Bold
                    }
                }

                Repeater {
                    model: [{ id: "now", label: "Now" }, { id: "rules", label: "Rules" }, { id: "lock", label: "Start a lock" },
                            { id: "lockdown", label: "Lock down this PC" }]
                    delegate: Button {
                        id: navItem
                        required property var modelData
                        readonly property bool current: window.screen === modelData.id
                        Layout.fillWidth: true
                        implicitHeight: 44
                        text: modelData.label
                        hoverEnabled: true
                        Accessible.role: Accessible.PageTab
                        Accessible.selected: current
                        onClicked: window.screen = modelData.id
                        contentItem: Text {
                            text: navItem.text
                            leftPadding: 4
                            color: navItem.current ? Theme.text : Theme.muted
                            font.pixelSize: 15
                            font.weight: navItem.current ? Font.DemiBold : Font.Medium
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 12
                            color: navItem.current ? Theme.accentSoft : navItem.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
                            border.color: navItem.current ? Theme.edgeStrong : navItem.visualFocus ? Theme.accent : "transparent"
                            border.width: navItem.visualFocus ? 2 : 1
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    text: "Runs only on this PC.\nNothing leaves it."
                    color: Theme.muted
                    font.pixelSize: 13
                }
            }
        }

        // ---- Content ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 32
            spacing: 24

            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                Button {
                    id: paletteButton
                    Layout.fillWidth: true
                    Layout.maximumWidth: 520
                    implicitHeight: 44
                    leftPadding: 16
                    rightPadding: 12
                    hoverEnabled: true
                    Accessible.name: "Search or type a command, Control K"
                    onClicked: commandPalette.open()
                    contentItem: RowLayout {
                        spacing: 12
                        Text { Layout.fillWidth: true; text: "Search or type a command…"; color: Theme.muted; elide: Text.ElideRight }
                        Rectangle {
                            implicitWidth: kbd.implicitWidth + 16
                            implicitHeight: 22
                            radius: 6
                            color: "transparent"
                            border.color: Qt.rgba(0.62, 0.71, 0.84, 0.35)
                            Text { id: kbd; anchors.centerIn: parent; text: "Ctrl K"; color: Theme.muted; font.pixelSize: 12 }
                        }
                    }
                    background: Rectangle {
                        radius: 12
                        color: Qt.rgba(4 / 255, 10 / 255, 22 / 255, 0.55)
                        border.color: paletteButton.hovered || paletteButton.visualFocus ? Theme.accent : Qt.rgba(0.36, 0.88, 0.9, 0.22)
                        border.width: paletteButton.visualFocus ? 2 : 1
                    }
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    id: statusPill
                    readonly property var locks: Service.status.locked || []
                    readonly property string label: !Service.connected ? "Not connected"
                                                    : locks.length ? `Locked until ${Theme.clock(Date.parse(locks[0].until))}`
                                                    : Service.status.dry_run ? "Dry run: nothing is enforced" : "Enforcing"
                    readonly property color tone: !Service.connected ? Theme.muted
                                                  : locks.length ? Theme.accent : Service.status.dry_run ? Theme.amber : Theme.good
                    implicitWidth: statusRow.implicitWidth + 28
                    implicitHeight: 36
                    radius: 18
                    color: "transparent"
                    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.45)
                    Accessible.role: Accessible.StaticText
                    Accessible.name: label
                    RowLayout {
                        id: statusRow
                        anchors.centerIn: parent
                        spacing: 8
                        Rectangle { width: 8; height: 8; radius: 4; color: statusPill.tone }
                        Text { text: statusPill.label; color: statusPill.tone; font.pixelSize: 14; font.weight: Font.DemiBold }
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: ["now", "rules", "lock", "lockdown"].indexOf(window.screen)

                NowScreen {
                    now: window.now
                    onStartLock: window.screen = "lock"
                    onLockDown: window.screen = "lockdown"
                    onMessage: text => window.say(text)
                    onReport: (result, success) => window.report(result, success)
                }
                RulesScreen {
                    id: rulesScreen
                    now: window.now
                    onReport: (result, success) => window.report(result, success)
                }
                LockScreen {
                    id: lockScreen
                    now: window.now
                    onLocked: window.screen = "now"
                    onReport: (result, success) => window.report(result, success)
                }
                LockdownScreen {
                    onDone: window.screen = "now"
                }
            }
        }
    }

    // ---- The service isn't reachable ----
    Rectangle {
        anchors.fill: parent
        visible: !Service.connected
        color: Qt.rgba(2 / 255, 6 / 255, 14 / 255, 0.86)
        MouseArea { anchors.fill: parent }  // nothing behind is usable meanwhile

        Card {
            anchors.centerIn: parent
            width: Math.min(520, parent.width - 48)
            Text { text: "Waiting for the service"; color: Theme.text; font.family: Theme.display; font.pixelSize: 24; font.weight: Font.DemiBold }
            Text {
                Layout.fillWidth: true
                text: Service.problem.length ? `${Service.problem.charAt(0).toUpperCase()}${Service.problem.slice(1)}.` : ""
                color: Theme.text; wrapMode: Text.Wrap
            }
            Text {
                Layout.fillWidth: true
                text: "It starts with Windows once Sirenproof is installed. This window reconnects by itself."
                color: Theme.muted; wrapMode: Text.Wrap; font.pixelSize: 14
            }
        }
    }

    // ---- Toast ----
    Rectangle {
        id: toast
        property bool shown: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: shown ? 28 : 12
        width: Math.min(toastText.implicitWidth + 40, parent.width - 48)
        height: toastText.implicitHeight + 24
        radius: 14
        color: "#0f2342"
        border.color: Theme.edgeStrong
        opacity: shown ? 1 : 0
        Behavior on opacity { enabled: Service.animations; NumberAnimation { duration: 180 } }
        Behavior on anchors.bottomMargin { enabled: Service.animations; NumberAnimation { duration: 180 } }
        Accessible.role: Accessible.AlertMessage
        Accessible.name: toastText.text
        Text {
            id: toastText
            anchors.centerIn: parent
            width: parent.width - 40
            color: Theme.text
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
        Timer { id: toastTimer; interval: 4200; onTriggered: toast.shown = false }
    }
}
