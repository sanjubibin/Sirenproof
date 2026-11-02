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
    property int minutes: 120
    property string method: "wait"

    readonly property var setDefs: [
        { id: "video", label: "Video", detail: "YouTube home, feed and Shorts", targets: ["youtube.com/", "youtube.com/feed", "youtube.com/shorts"] },
        { id: "social", label: "Social", detail: "Reddit, X, Instagram", targets: ["reddit.com", "x.com", "instagram.com"] },
        { id: "games", label: "Games", detail: "Steam, Discord", targets: ["app:steam.exe", "app:discord.exe"] }
    ]
    readonly property var targets: setDefs.filter(s => sets[s.id]).reduce((all, s) => all.concat(s.targets), [])
    readonly property bool lockActive: (Service.status.locked || []).length > 0
    readonly property bool wide: width >= 820

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

    component Choice: Button {
        id: choice
        property string detail
        property bool chosen
        Layout.fillWidth: true
        padding: 14
        hoverEnabled: true
        Accessible.checked: chosen
        contentItem: ColumnLayout {
            spacing: 2
            Text { text: choice.text; color: Theme.text; font.weight: Font.DemiBold }
            Text { text: choice.detail; color: Theme.muted; font.pixelSize: 13; visible: text.length > 0 }
        }
        background: Rectangle {
            radius: 14
            color: choice.chosen ? Theme.accentSoft : "transparent"
            border.color: choice.chosen || choice.visualFocus ? Theme.accent : Theme.edge
            border.width: choice.visualFocus ? 2 : 1
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
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Repeater {
                        model: root.setDefs
                        delegate: Choice {
                            required property var modelData
                            text: modelData.label
                            detail: modelData.detail
                            chosen: root.sets[modelData.id]
                            onClicked: {
                                const next = Object.assign({}, root.sets)
                                next[modelData.id] = !next[modelData.id]
                                root.sets = next
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: "FOR HOW LONG"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
                RowLayout {
                    spacing: 10
                    Repeater {
                        model: [{ m: 25, label: "25 min" }, { m: 60, label: "1 h" }, { m: 120, label: "2 h" }, { m: 240, label: "4 h" }]
                        delegate: Choice {
                            required property var modelData
                            Layout.fillWidth: false
                            Layout.preferredWidth: 96
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
                model: [
                    { text: `Locks until ${Theme.clock(root.now + root.minutes * 60000)}`, tone: Theme.text },
                    root.sets.video ? { text: "Blocks YouTube's home page, feed and Shorts. Videos still play.", tone: Theme.text } : null,
                    root.sets.social ? { text: "Blocks reddit.com, x.com and instagram.com", tone: Theme.text } : null,
                    root.sets.games ? { text: "Closes Steam and Discord. Anything already open gets 10 seconds to save.", tone: Theme.amber } : null,
                    { text: "Browsers without Sirenproof's extension close after a warning.", tone: Theme.muted }
                ].filter(item => item !== null)
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
            Text { text: friendPopup.code; color: Theme.text; font.family: "Cascadia Mono"; font.pixelSize: 28 }
            Text {
                Layout.maximumWidth: 420
                text: "Give it to someone you trust now. It's shown only this once and can't be recovered."
                color: Theme.muted
                wrapMode: Text.Wrap
            }
            SirenButton {
                kind: "primary"
                text: "I've handed it over"
                onClicked: { friendPopup.close(); friendPopup.code = ""; root.locked() }
            }
        }
    }
}
