import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sirenproof.Backend

// The type-it-out unlock: copy a long random text by hand. The text lights up as you get it right and
// marks the first character that doesn't match. Only typing counts: pasting is off, and so is anything
// else that drops in more than a character at once (the context menu, dragging text in). The service
// keeps the same text until it's typed, so closing the window doesn't deal a new one.
ColumnLayout {
    id: root

    required property var lock  // the lock from the service's status
    signal message(string text)
    signal report(var result, string success)
    signal unlocked()

    property string challenge: ""
    readonly property string target: normalise(challenge)
    readonly property string typedCode: normalise(typed.text)
    readonly property int correct: {  // how much of the text is right so far
        let n = 0
        while (n < typedCode.length && n < target.length && typedCode[n] === target[n]) ++n
        return n
    }
    readonly property bool mistake: correct < typedCode.length
    readonly property bool complete: target.length > 0 && typedCode === target

    spacing: 10

    // How many characters went in between `before` and `after`: what's left once the parts they share at
    // the start and the end are taken away. Typing puts in one at a time, even over a selection.
    function inserted(before, after) {
        let start = 0
        while (start < before.length && start < after.length && before[start] === after[start]) ++start
        let end = 0
        while (end < before.length - start && end < after.length - start
               && before[before.length - 1 - end] === after[after.length - 1 - end]) ++end
        return after.length - start - end
    }

    // As the service reads it: case, dashes, spaces and line breaks don't matter, and o/i/l count as 0/1.
    function normalise(text) {
        return text.toLowerCase().replace(/[\s-]/g, "").replace(/o/g, "0").replace(/[il]/g, "1")
    }

    // The text in groups of five, done in sea glass and the first wrong character in amber.
    function marked() {
        let html = ""
        let index = 0
        for (const c of challenge) {
            if (c === "-") { html += " "; continue }
            if (index < correct) html += `<span style="color:${Theme.accent}">${c}</span>`
            else if (index === correct && mistake) html += `<span style="color:${Theme.inkOnAccent};background-color:${Theme.amber}">${c}</span>`
            else html += c
            ++index
        }
        return html
    }

    function begin() {
        const result = Service.beginTypeUnlock(lock.rule)
        if (result.text) {
            challenge = result.text
            typed.forceActiveFocus()
        } else {
            report(result, "")
        }
    }

    function finish() {
        if (!complete) return
        const result = Service.finishTypeUnlock(lock.rule, typed.text)
        if (result.ok) {
            challenge = ""
            typed.clear()
            unlocked()
        }
        report(result, "Unlocked. The rules you scheduled still apply.")
    }

    // A different lock means a different text.
    onLockChanged: if (!lock || lock.unlock !== "type") { challenge = ""; typed.clear() }

    SirenButton {
        visible: root.challenge.length === 0
        kind: "amber"
        text: `Show the text to type (${root.lock ? root.lock.type_length || 200 : 200} characters)`
        onClicked: root.begin()
    }

    Rectangle {
        Layout.fillWidth: true
        visible: root.challenge.length > 0
        implicitHeight: shown.implicitHeight + 24
        radius: 12
        color: Theme.sunken
        border.color: Theme.edge

        Text {
            id: shown
            anchors.fill: parent
            anchors.margins: 12
            text: root.marked()
            textFormat: Text.RichText  // StyledText has no background colour for the marked character
            color: Theme.text
            wrapMode: Text.Wrap
            lineHeight: 1.3
            font.family: "Cascadia Mono"; font.pixelSize: 16; font.letterSpacing: 0.6
            Accessible.role: Accessible.StaticText
            Accessible.name: "The text to type: " + root.challenge
        }
    }

    // Progress: a thin bar that fills as the text gets typed correctly.
    ColumnLayout {
        Layout.fillWidth: true
        visible: root.challenge.length > 0
        spacing: 6

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 4
            radius: 2
            color: Theme.edge
            Rectangle {
                width: root.target.length ? parent.width * root.correct / root.target.length : 0
                height: parent.height
                radius: 2
                color: root.mistake ? Theme.amber : Theme.accent
                Behavior on width { enabled: Service.animations; NumberAnimation { duration: 150 } }
            }
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: root.mistake ? Theme.amber : Theme.muted
            font.pixelSize: 13; font.features: { "tnum": 1 }
            text: root.complete ? "All of it matches. Press Unlock, or Enter."
                : root.mistake ? `Character ${root.correct + 1} doesn't match. Fix it to go on.`
                : `${root.correct} of ${root.target.length} characters`
        }
    }

    TextArea {
        id: typed
        property string accepted: ""  // the text as last typed, to go back to if something drops in

        Layout.fillWidth: true
        Layout.minimumHeight: 96
        visible: root.challenge.length > 0
        placeholderText: "Type it here. Pasting is turned off."
        wrapMode: TextEdit.WrapAnywhere
        color: Theme.text
        font.family: "Cascadia Mono"; font.pixelSize: 15
        persistentSelection: false
        Accessible.name: "Type the text shown above"
        background: Rectangle {
            radius: 12
            color: Theme.sunken
            border.color: root.mistake ? Theme.amberEdge : typed.activeFocus ? Theme.accent : Theme.edgeStrong
        }

        onTextChanged: {
            if (root.inserted(accepted, text) > 1) {
                text = accepted  // comes back through here, and is accepted
                cursorPosition = text.length
                root.message("Only typing counts here. Pasting and dropping text in are turned off.")
            } else {
                accepted = text
            }
        }
        Keys.onPressed: event => {
            if (event.matches(StandardKey.Paste)) {
                event.accepted = true
                root.message("Pasting is turned off here. Type it out.")
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                event.accepted = true  // no line breaks; Enter unlocks once it all matches
                root.finish()
            }
        }
    }

    RowLayout {
        visible: root.challenge.length > 0
        spacing: 10
        SirenButton {
            kind: "amber"
            text: "Unlock"
            enabled: root.complete
            onClicked: root.finish()
        }
        SirenButton {
            text: "Start over"
            visible: typed.text.length > 0
            onClicked: { typed.clear(); typed.forceActiveFocus() }
        }
    }
}
