import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sirenproof.Backend

// Tie yourself to the mast: choose what, for how long and how it can end early, see exactly what will
// happen, then press and hold to commit.
Flickable {
    id: root

    required property real now
    signal locked()
    signal report(var result, string success)

    property var sets: ({ video: true, social: true, games: false })
    property var extra: []      // targets beyond the ready-made sets, e.g. from the command palette
    property int minutes: 120
    property string method: "wait"

    readonly property var setDefs: Service.readyLists
    readonly property var lengths: [{ m: 25, label: "25 min" }, { m: 60, label: "1 h" }, { m: 120, label: "2 h" }, { m: 240, label: "4 h" }]
    readonly property var targets: setDefs.filter(s => sets[s.id]).reduce((all, s) => all.concat(s.targets), [])
                                          .concat(extra.filter(t => !setDefs.some(s => sets[s.id] && s.targets.includes(t))))
    readonly property var extraSites: extra.filter(t => !t.includes(":"))
    readonly property var extraApps: extra.filter(t => t.includes(":")).map(t => t.slice(t.indexOf(":") + 1))
    readonly property bool lockActive: (Service.status.locked || []).length > 0
    readonly property bool wide: width >= 820

    // Fills the screen in from the command palette. Nothing starts until the button is held.
    function prefill(lists, others, length, unlock) {
        if (lists.length || others.length) {
            const chosen = {}
            for (const id of lists) chosen[id] = true
            sets = chosen
            extra = others
        }
        if (length > 0) minutes = length
        if (unlock) method = unlock
        contentY = 0
    }

    function lengthLabel(m) {
        return m < 60 ? `${m} min` : m % 60 === 0 ? `${m / 60} h` : `${Math.floor(m / 60)} h ${m % 60} min`
    }

    function sentenceList(names) {
        return names.length > 1 ? `${names.slice(0, -1).join(", ")} and ${names[names.length - 1]}` : names[0]
    }

    contentHeight: layout.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    function commit() {
        const result = Service.startLock(targets, minutes, method, goal.text.trim())
        if (result.friend_code) {
            friendPopup.code = result.friend_code
            friendPopup.open()
        }
        if (result.outcome === "applied") {
            root.report(result, `Locked until ${Theme.clock(Date.now() + minutes * 60000)}. You chose this; it will hold.`)
            if (!result.friend_code) root.locked()
        } else {
            root.report(result, "")
        }
    }

    GridLayout {
        id: layout
        width: root.width
        columns: root.wide ? 2 : 1
        columnSpacing: 24
        rowSpacing: 24

        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 22

            Text { text: "Tie yourself to the mast"; color: Theme.text; font.family: Theme.display; font.pixelSize: 26; font.weight: Font.DemiBold }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: "WHAT TO LOCK"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
                GridLayout {
                    Layout.fillWidth: true
                    columns: width >= 560 ? root.setDefs.length : 2
                    columnSpacing: 10
                    rowSpacing: 10
                    Repeater {
                        model: root.setDefs
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
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: "FOR HOW LONG"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
                Flow {
                    Layout.fillWidth: true
                    spacing: 10
                    Repeater {
                        // A length typed in the palette shows as one more choice.
                        model: root.lengths.some(l => l.m === root.minutes) ? root.lengths
                                                                             : root.lengths.concat([{ m: root.minutes, label: root.lengthLabel(root.minutes) }])
                        delegate: Choice {
                            required property var modelData
                            width: Math.max(96, implicitWidth)
                            text: modelData.label
                            chosen: root.minutes === modelData.m
                            onClicked: root.minutes = modelData.m
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: "IF YOU NEED OUT EARLY"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 10
                    rowSpacing: 10
                    Repeater {
                        model: [
                            { id: "none", label: "No way out", detail: "It ends when it ends" },
                            { id: "wait", label: "Wait 30 minutes", detail: "Ask, then wait it out" },
                            { id: "type", label: "Type it out", detail: "Copy 200 random characters" },
                            { id: "friend", label: "A friend's code", detail: "Shown once, given away" }
                        ]
                        delegate: Choice {
                            required property var modelData
                            text: modelData.label
                            detail: modelData.detail
                            chosen: root.method === modelData.id
                            onClicked: root.method = modelData.id
                        }
                    }
                }
            }

            TextField {
                id: goal
                Layout.fillWidth: true
                placeholderText: "What is this lock for? (shown when something is blocked)"
                color: Theme.text
                Accessible.name: "Goal"
                background: Rectangle { radius: 12; color: Theme.sunken; border.color: goal.activeFocus ? Theme.accent : Theme.edgeStrong; implicitHeight: 44 }
            }
        }

        Card {
            Layout.fillWidth: true
            Layout.preferredWidth: root.wide ? 340 : -1
            Layout.maximumWidth: root.wide ? 380 : 100000
            Layout.alignment: Qt.AlignTop
            spacing: 14

            Text { text: "BEFORE YOU COMMIT"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
            Repeater {
                model: [{ text: `Locks until ${Theme.clock(root.now + root.minutes * 60000)}`, tone: Theme.text }]
                    .concat(root.setDefs.filter(s => root.sets[s.id]).map(s => ({
                        text: s.effect,
                        tone: s.targets.some(t => t.startsWith("app:")) ? Theme.amber : Theme.text
                    })))
                    .concat(root.extraSites.length ? [{ text: `Blocks ${root.sentenceList(root.extraSites)}.`, tone: Theme.text }] : [])
                    .concat(root.extraApps.length ? [{ text: `Closes ${root.sentenceList(root.extraApps)}. Anything already open gets 10 seconds to save.`, tone: Theme.amber }] : [])
                    .concat([{ text: "Browsers without Sirenproof's extension close after a warning.", tone: Theme.muted }])
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: line.implicitHeight + 20
                    radius: 12
                    color: Theme.sunken
                    Text {
                        id: line
                        anchors.fill: parent
                        anchors.margins: 10
                        text: parent.modelData.text
                        color: parent.modelData.tone
                        wrapMode: Text.Wrap
                        font.pixelSize: 14
                    }
                }
            }

            Item {
                id: hold
                property real progress: 0
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                implicitWidth: 136
                implicitHeight: 136

                NumberAnimation {
                    id: filling
                    target: hold
                    property: "progress"
                    from: 0
                    to: 1
                    duration: 2000
                    onFinished: root.commit()
                }

                MastRing {
                    anchors.fill: parent
                    thickness: 8
                    progress: hold.progress
                    animated: false
                }

                Button {
                    id: holdButton
                    anchors.centerIn: parent
                    width: 100
                    height: 100
                    enabled: root.targets.length > 0 && !root.lockActive
                    text: !root.targets.length ? "Pick something" : hold.progress > 0 ? "Keep holding…" : "Hold to lock"
                    Accessible.description: "Press and hold for two seconds to start the lock"
                    onDownChanged: {
                        if (down) {
                            filling.restart()
                        } else {
                            filling.stop()
                            hold.progress = 0
                        }
                    }
                    contentItem: Text {
                        text: holdButton.text
                        color: Theme.inkOnAccent
                        font.weight: Font.Bold
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: width / 2
                        color: Theme.accent
                        opacity: holdButton.enabled ? 1 : 0.45
                        border.width: holdButton.visualFocus ? 3 : 0
                        border.color: Theme.text
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.lockActive ? "A lock is already holding." : "Press and hold for 2 seconds. A lock can't be loosened until it ends."
                color: Theme.muted
                font.pixelSize: 13
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Popup {
        id: friendPopup
        property string code: ""
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 28
        closePolicy: Popup.NoAutoClose
        background: Rectangle { radius: Theme.radius; color: "#0a1a33"; border.color: Theme.accent }
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "YOUR FRIEND CODE"; color: Theme.accent; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
            Text {
                // Upper case, groups spaced apart: easiest to read out over the phone or copy onto paper.
                text: friendPopup.code.toUpperCase().split("-").join("  ")
                color: Theme.text
                font.family: "Cascadia Mono"; font.pixelSize: 30; font.letterSpacing: 2
                Accessible.role: Accessible.StaticText
                Accessible.name: "Friend code: " + friendPopup.code.split("").join(" ")
            }
            Text {
                Layout.maximumWidth: 440
                text: "Give it to someone you trust now: read it out, or write it down for them. It's shown only this once and can't be recovered. Don't keep a copy on this PC."
                color: Theme.muted
                wrapMode: Text.Wrap
            }
            CheckBox {
                id: handedOver
                text: "Someone else has the code now"
                Accessible.name: text
                contentItem: Text {
                    leftPadding: handedOver.indicator.width + handedOver.spacing
                    text: handedOver.text
                    color: Theme.text
                    verticalAlignment: Text.AlignVCenter
                }
            }
            SirenButton {
                kind: "primary"
                text: "Done"  // the lock already holds; this only puts the code away
                // Closing this loses the code for good, so it takes a deliberate tick first.
                enabled: handedOver.checked
                onClicked: { friendPopup.close(); friendPopup.code = ""; handedOver.checked = false; root.locked() }
            }
        }
    }
}
