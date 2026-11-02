import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sirenproof.Backend

// Your rules, and one rule up close: what it blocks, how strongly, when, and a live tester.
Flickable {
    id: root

    signal report(var result, string success)

    property string selected: ""
    readonly property var rules: Service.rules || []
    readonly property var rule: rules.find(r => r.id === selected) || rules[0] || null
    readonly property bool wide: width >= 820

    contentHeight: layout.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    function dayRange(list) {
        const names = ["mon", "tue", "wed", "thu", "fri", "sat", "sun"]
        const label = d => d.charAt(0).toUpperCase() + d.slice(1)
        const idx = list.map(d => names.indexOf(d)).sort((a, b) => a - b)
        const contiguous = idx.every((v, i) => i === 0 || v === idx[i - 1] + 1)
        if (contiguous && idx.length > 2) return `${label(names[idx[0]])}–${label(names[idx[idx.length - 1]])}`
        return idx.map(i => label(names[i])).join(", ")
    }

    function when(r) {
        if (!r || !r.when || r.when === "always") return "Always"
        if (r.when.weekly) return r.when.weekly.map(w => `${dayRange(w.days)} · ${w.from}–${w.to}`).join("; ")
        return `Session until ${Theme.clock(Date.parse(r.when.to))}`
    }

    function tone(strength) {
        return strength === "lock" ? Theme.accent : strength === "nudge" ? Theme.amber : Theme.muted
    }

    GridLayout {
        id: layout
        width: root.width
        columns: root.wide ? 2 : 1
        columnSpacing: 24
        rowSpacing: 24

        // ---- The list ----
        ColumnLayout {
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: root.wide ? 300 : -1
            Layout.fillWidth: !root.wide
            spacing: 12

            Repeater {
                model: root.rules
                delegate: Button {
                    id: item
                    required property var modelData
                    readonly property bool current: root.rule !== null && root.rule.id === modelData.id
                    Layout.fillWidth: true
                    padding: 14
                    hoverEnabled: true
                    Accessible.name: `${modelData.name || modelData.id}, ${modelData.strength}`
                    onClicked: root.selected = modelData.id
                    contentItem: ColumnLayout {
                        spacing: 2
                        RowLayout {
                            Text {
                                Layout.fillWidth: true
                                text: item.modelData.name || item.modelData.id
                                color: Theme.text; font.weight: Font.DemiBold; elide: Text.ElideRight
                            }
                            Text {
                                text: item.modelData.strength.toUpperCase()
                                color: root.tone(item.modelData.strength)
                                font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.4
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: root.when(item.modelData)
                            color: Theme.muted; font.pixelSize: 13; elide: Text.ElideRight
                        }
                    }
                    background: Rectangle {
                        radius: 14
                        color: item.current ? Theme.accentSoft : item.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
                        border.color: item.current || item.visualFocus ? Theme.accent : Theme.edge
                        border.width: item.visualFocus ? 2 : 1
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: root.rules.length === 0
                text: "No rules yet. Start a lock, or add one from the command line:\nsirenproof block reddit.com --for 25m"
                color: Theme.muted
                wrapMode: Text.Wrap
            }
        }

        // ---- One rule ----
        Card {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            visible: root.rule !== null
            spacing: 22

            ColumnLayout {
                spacing: 4
                Text {
                    text: root.rule ? `${root.rule.strength.toUpperCase()} RULE` : ""
                    color: root.rule ? root.tone(root.rule.strength) : Theme.muted
                    font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6
                }
                Text {
                    text: root.rule ? (root.rule.name || root.rule.id) : ""
                    color: Theme.text; font.family: Theme.display; font.pixelSize: 26; font.weight: Font.DemiBold
                }
                Text {
                    visible: root.rule !== null && (root.rule.goal || "").length > 0
                    text: root.rule ? `Goal: ${root.rule.goal}` : ""
                    color: Theme.muted
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10
                Text { text: "BLOCKS"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: root.rule ? root.rule.block : []
                        delegate: Rectangle {
                            id: chip
                            required property string modelData
                            width: chipRow.implicitWidth + 18
                            height: 34
                            radius: 17
                            color: Theme.accentSoft
                            border.color: Theme.edgeStrong
                            RowLayout {
                                id: chipRow
                                anchors.verticalCenter: parent.verticalCenter
                                x: 12
                                spacing: 2
                                Text { text: chip.modelData; color: Theme.text; font.pixelSize: 14 }
                                Button {
                                    implicitWidth: 28; implicitHeight: 28
                                    text: "×"
                                    Accessible.name: `Remove ${chip.modelData}`
                                    onClicked: root.report(Service.removeTarget(root.rule.id, chip.modelData), `Removed ${chip.modelData}.`)
                                    contentItem: Text { text: "×"; color: Theme.muted; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { radius: 14; color: parent.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent" }
                                }
                            }
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    TextField {
                        id: newTarget
                        Layout.fillWidth: true
                        placeholderText: "reddit.com, youtube.com/shorts or app:steam.exe"
                        color: Theme.text
                        Accessible.name: "Add a site, page or app"
                        background: Rectangle { radius: 12; color: Theme.sunken; border.color: newTarget.activeFocus ? Theme.accent : Theme.edgeStrong; implicitHeight: 44 }
                        onAccepted: addButton.clicked()
                    }
                    SirenButton {
                        id: addButton
                        kind: "primary"
                        text: "Add"
                        enabled: newTarget.text.trim().length > 0
                        onClicked: {
                            const target = newTarget.text.trim()
                            const result = Service.addTarget(root.rule.id, target)
                            if (!result.error && result.outcome !== "refused") newTarget.text = ""
                            root.report(result, `Added ${target}. Tightening applies at once.`)
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: 10
                Text { text: "STRENGTH"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
                Row {
                    Repeater {
                        model: ["nudge", "block", "lock"]
                        delegate: Button {
                            id: segment
                            required property string modelData
                            required property int index
                            readonly property bool current: root.rule !== null && root.rule.strength === modelData
                            implicitHeight: 44
                            leftPadding: 22; rightPadding: 22
                            text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                            Accessible.checked: current
                            onClicked: if (!current) root.report(Service.setStrength(root.rule.id, modelData), "Stronger: applied at once.")
                            contentItem: Text {
                                text: segment.text
                                color: segment.current ? Theme.inkOnAccent : Theme.muted
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: segment.current ? Theme.accent : "transparent"
                                border.color: segment.visualFocus ? Theme.accent : Theme.edgeStrong
                                border.width: segment.visualFocus ? 2 : 1
                                topLeftRadius: segment.index === 0 ? 22 : 0
                                bottomLeftRadius: segment.index === 0 ? 22 : 0
                                topRightRadius: segment.index === 2 ? 22 : 0
                                bottomRightRadius: segment.index === 2 ? 22 : 0
                            }
                        }
                    }
                }
                Text {
                    text: "Tightening applies at once. Loosening waits for the rule's delay, and a lock refuses it."
                    color: Theme.muted; font.pixelSize: 13
                }
            }

            ColumnLayout {
                spacing: 6
                Text { text: "WHEN"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
                Text { text: root.when(root.rule); color: Theme.text }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: testerColumn.implicitHeight + 32
                radius: 16
                color: Theme.sunken

                ColumnLayout {
                    id: testerColumn
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10
                    Text { text: "RULE TESTER"; color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6 }
                    TextField {
                        id: tester
                        Layout.fillWidth: true
                        text: "https://www.youtube.com/shorts/abc"
                        color: Theme.text
                        Accessible.name: "Web address or program to test"
                        background: Rectangle { radius: 12; color: Qt.rgba(0, 0, 0, 0.2); border.color: tester.activeFocus ? Theme.accent : Theme.edgeStrong; implicitHeight: 44 }
                        onTextChanged: testDelay.restart()
                    }
                    Text {
                        id: verdict
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        font.weight: Font.DemiBold
                        Accessible.role: Accessible.StaticText
                    }
                    Timer {
                        id: testDelay
                        interval: 250
                        running: true
                        onTriggered: {
                            const v = Service.check(tester.text)
                            if (v.error) { verdict.text = v.error; verdict.color = Theme.muted; return }
                            if (v.action === "allow") { verdict.text = "Allowed right now"; verdict.color = Theme.good; return }
                            verdict.text = `${v.action === "nudge" ? "Nudge" : "Blocked"} by “${Theme.ruleName(Service.rules, v.rule)}” (${v.strength})`
                            verdict.color = v.action === "nudge" ? Theme.amber : Theme.bad
                        }
                    }
                }
            }

            SirenButton {
                kind: "amber"
                text: "Remove this rule"
                onClicked: root.report(Service.removeRule(root.rule.id), "Removed.")
            }
        }
    }
}
