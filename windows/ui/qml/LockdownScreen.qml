import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Sirenproof.Backend

// Lock down this PC (plan §3.8): move everyday use to a standard account, so that switching Sirenproof
// off needs a password someone else holds. Sirenproof only checks and explains. Every change happens in
// Windows' own settings, and a step that could leave the PC with no administrator waits until it's safe.
Flickable {
    id: root

    signal done()

    property int step: 0
    property var accounts: Accounts.read()

    readonly property bool known: !accounts.error
    readonly property bool admin: known && accounts.admin
    readonly property var others: known ? accounts.others : []
    readonly property string user: known ? accounts.user : "your account"
    readonly property string keeper: others.length ? others[0] : "the new account"

    readonly property var steps: [
        {
            title: "Check who is in charge",
            body: !known ? `Sirenproof couldn't read this PC's accounts: ${accounts.error}.`
                : admin ? `Your everyday account, ${user}, is an administrator. That means it can switch Sirenproof off, and so can you, in a weak moment.`
                : `Your account, ${user}, is already a standard user, so switching Sirenproof off needs an administrator's password. There's nothing more to do here.`,
            status: known ? (others.length ? `Other administrators on this PC: ${others.join(", ")}.` : "There's no other administrator account yet.") : "",
            tone: Theme.muted,
            why: "The service already runs above any administrator. But Windows lets an administrator remove anything in the end, so the strongest protection is not being one day to day.",
            next: known && !admin ? "Done" : "Continue",
            canNext: known
        },
        {
            title: "Create a separate administrator",
            body: "Open Windows' account settings and choose Add account. Pick “I don't have this person's sign-in information”, then “Add a user without a Microsoft account”. Call it Keeper, or anything you like, and give it a long password. Then select it, choose “Change account type” and make it an Administrator.",
            action: "Open account settings",
            run: () => Accounts.openAccountSettings(),
            status: others.length ? `Found: ${others.join(", ")}. ✓` : "Waiting for a second administrator account. This page checks every few seconds.",
            tone: others.length ? Theme.good : Theme.muted,
            why: "Sirenproof never creates or changes accounts by itself. Every step happens in Windows' own settings, where you can see it.",
            next: "I've created it",
            canNext: others.length > 0
        },
        {
            title: "Give the password away",
            body: `First make sure the password works: sign in as ${keeper} once (Windows finishes setting the account up then) and come back. Then hand the password to someone you trust, or write it down and seal it in an envelope somewhere inconvenient.`,
            status: "",
            tone: Theme.muted,
            why: "Like a friend code: whoever holds it never has to do anything unless you ask. Keep it where it can't be lost; it's also how you'd undo this.",
            next: "It's out of my hands",
            canNext: true
        },
        {
            title: "Become a standard user",
            body: `Open User Accounts, choose “Change your account type” and pick Standard. Windows asks for ${keeper}'s password. Or sign in as ${keeper} and change ${user} from Settings › Accounts › Other users.`,
            action: others.length ? "Open User Accounts" : "",
            run: () => Accounts.openUserAccounts(),
            status: !admin ? `Done: ${user} is now a standard user. ✓`
                : others.length ? `${user} is still an administrator.`
                : "There's no other administrator any more. Don't change your account type until there is one, or nobody could look after this PC.",
            tone: !admin ? Theme.good : others.length ? Theme.muted : Theme.amber,
            why: "From now on, stopping Sirenproof, editing the hosts file, changing the clock or uninstalling all need that password. To undo this, sign in as the administrator and change your account back.",
            next: admin ? "Finish later" : "Finish",
            canNext: true
        }
    ]
    readonly property var current: steps[step]

    function advance() {
        if ((step === 0 && known && !admin) || step === steps.length - 1) {
            step = 0
            done()
        } else {
            step += 1
        }
    }

    contentHeight: layout.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    // Accounts change in another window, so look again every few seconds while this screen shows.
    Timer {
        interval: 3000
        running: root.visible
        repeat: true
        onTriggered: root.accounts = Accounts.read()
    }
    onVisibleChanged: if (visible) accounts = Accounts.read()

    ColumnLayout {
        id: layout
        width: Math.min(root.width, 760)

        Card {
            Layout.fillWidth: true
            spacing: 20

            ColumnLayout {
                spacing: 6
                Text {
                    text: `LOCK DOWN THIS PC · STEP ${root.step + 1} OF ${root.steps.length}`
                    color: Theme.muted; font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 1.6
                }
                Text {
                    text: root.current.title
                    color: Theme.text; font.family: Theme.display; font.pixelSize: 26; font.weight: Font.DemiBold
                    Accessible.role: Accessible.Heading
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Repeater {
                    model: root.steps.length
                    delegate: Rectangle {
                        required property int index
                        Layout.fillWidth: true
                        height: 4
                        radius: 2
                        color: index <= root.step ? Theme.accent : Qt.rgba(0.62, 0.71, 0.84, 0.2)
                        Behavior on color { enabled: Service.animations; ColorAnimation { duration: 200 } }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.current.body
                color: Theme.text
                font.pixelSize: 16
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                visible: !!root.current.action || root.current.status.length > 0
                SirenButton {
                    visible: !!root.current.action
                    text: root.current.action || ""
                    onClicked: root.current.run()
                }
                Text {
                    Layout.fillWidth: true
                    text: root.current.status
                    color: root.current.tone
                    wrapMode: Text.Wrap
                    Accessible.role: Accessible.StaticText
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: why.implicitHeight + 28
                radius: 14
                color: Theme.sunken
                Text {
                    id: why
                    anchors.fill: parent
                    anchors.margins: 14
                    text: root.current.why
                    color: Theme.muted
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                }
            }

            RowLayout {
                spacing: 12
                SirenButton {
                    text: "Back"
                    enabled: root.step > 0
                    onClicked: root.step -= 1
                }
                SirenButton {
                    kind: "primary"
                    text: root.current.next
                    enabled: root.current.canNext
                    onClicked: root.advance()
                }
            }
        }
    }
}
