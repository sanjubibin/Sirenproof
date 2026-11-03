import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sirenproof.Backend

// The friend-code unlock: enter the code you handed to someone when the lock began. The field tidies
// the code as you type (groups of five, look-alike letters mapped back), says how many tries are left
// before a wait, and counts the wait down. Pasting is fine here: the code usually arrives in a message.
ColumnLayout {
    id: root

    required property var lock  // the lock from the service's status
    required property real now
    signal report(var result, string success)
    signal unlocked()

    readonly property int codeLength: 15
    readonly property string entered: code.text.replace(/-/g, "")
    readonly property real retryAfter: lock && lock.retry_after ? Date.parse(lock.retry_after) : 0
    readonly property bool waiting: retryAfter > now
    readonly property int triesLeft: lock && lock.tries_left !== undefined ? lock.tries_left : 5
    property string problem: ""

    spacing: 8

    // Only the code's own characters, as the service reads them, in groups of five.
    function tidy(text) {
        const clean = text.toLowerCase().replace(/o/g, "0").replace(/[il]/g, "1")
                          .replace(/[^0-9a-hjkmnp-tv-z]/g, "").slice(0, codeLength)
        return clean.replace(/(.{5})(?=.)/g, "$1-")
    }

    function submit() {
        if (entered.length !== codeLength || waiting) return
        const result = Service.enterFriendCode(lock.rule, code.text)
        if (result.ok) {
            code.clear()
            problem = ""
            unlocked()
        } else {
            problem = result.error || result.message || ""
            code.selectAll()
        }
        report(result, "Unlocked.")
    }

    onLockChanged: if (!lock || lock.unlock !== "friend") { code.clear(); problem = "" }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        TextField {
            id: code
            Layout.fillWidth: true
            enabled: !root.waiting
            placeholderText: "xxxxx-xxxxx-xxxxx"
            maximumLength: 17
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
            color: Theme.text
            font.family: "Cascadia Mono"; font.pixelSize: 18; font.letterSpacing: 1.2
            Accessible.name: "Friend code"
            Accessible.description: "Fifteen letters and digits; dashes are added for you"
            background: Rectangle {
                radius: 12
                color: Theme.sunken
                border.color: root.problem ? Theme.amberEdge : code.activeFocus ? Theme.accent : Theme.edgeStrong
                implicitHeight: 48
            }
            onTextEdited: {
                const tidied = root.tidy(text)
                if (tidied !== text) {
                    text = tidied
                    cursorPosition = tidied.length
                }
                root.problem = ""
            }
            onAccepted: root.submit()
        }
        SirenButton {
            kind: "amber"
            text: "Unlock"
            enabled: root.entered.length === root.codeLength && !root.waiting
            onClicked: root.submit()
        }
    }

    Text {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.pixelSize: 13; font.features: { "tnum": 1 }
        color: root.waiting || root.problem ? Theme.amber : Theme.muted
        text: root.waiting
              ? `Too many wrong codes. You can try again in ${Theme.span(root.retryAfter - root.now)}, at ${Theme.clock(root.retryAfter)}.`
              : root.problem && root.triesLeft < 5
              ? `That's not it. ${root.triesLeft} ${root.triesLeft === 1 ? "try" : "tries"} left, then a ${root.lock.next_wait_minutes}-minute wait.`
              : root.problem ? root.problem
              : `${root.entered.length} of ${root.codeLength} characters. Capitals and dashes don't matter.`
    }
}
