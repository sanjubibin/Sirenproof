import QtQuick
import Sirenproof.Backend

// The schedule painter: the week as 7 rows of 24 hours. Drag across it to paint a block of hours on (or
// off, if you start on an hour that's on). Arrow keys move, Space switches one hour. It only reports
// what was painted; the screen decides what to do with it.
Item {
    id: grid

    required property var when
    required property real now          // trusted time, ms
    property int offsetMinutes: 0       // the time zone the schedule runs on

    signal painted(var hours, bool on)

    readonly property var days: ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
    readonly property var dayNames: ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]
    readonly property var cells: Service.weekHours(when)
    readonly property int labelWidth: 44
    readonly property int headerHeight: 22
    readonly property int gap: 3
    readonly property real pitchX: (width - labelWidth) / 24
    readonly property real pitchY: 30

    // The drag in progress: from `anchor` to `reach`, painting on or off. -1 when not dragging.
    property int anchor: -1
    property int reach: -1
    property bool paintOn: true
    property int hovered: -1
    property int cursor: 9          // keyboard position, Monday 09:00 to start

    // What the pointer or keyboard is on, for a line under the grid.
    readonly property string caption: hovered >= 0 ? describe(hovered) : activeFocus ? describe(cursor) : ""

    readonly property int nowHour: {
        const local = new Date(now + offsetMinutes * 60000)
        return ((local.getUTCDay() + 6) % 7) * 24 + local.getUTCHours()
    }

    implicitHeight: headerHeight + 7 * pitchY
    activeFocusOnTab: true
    Accessible.role: Accessible.Table
    Accessible.name: `Weekly schedule. ${describe(cursor)}. Arrow keys move, Space switches the hour on or off.`

    function hourLabel(h) { return `${String(h).padStart(2, "0")}:00` }
    function describe(i) {
        const v = cells[i]
        return `${dayNames[Math.floor(i / 24)]} ${hourLabel(i % 24)}, ${v === 2 ? "on" : v === 1 ? "partly on" : "off"}`
    }

    function cellAt(x, y, clamp) {
        let col = Math.floor((x - labelWidth) / pitchX)
        let row = Math.floor((y - headerHeight) / pitchY)
        if (!clamp && (col < 0 || col > 23 || row < 0 || row > 6)) return -1
        col = Math.max(0, Math.min(23, col))
        row = Math.max(0, Math.min(6, row))
        return row * 24 + col
    }

    function inDrag(i) {
        if (anchor < 0) return false
        const d = Math.floor(i / 24), h = i % 24
        const d0 = Math.floor(anchor / 24), h0 = anchor % 24, d1 = Math.floor(reach / 24), h1 = reach % 24
        return d >= Math.min(d0, d1) && d <= Math.max(d0, d1) && h >= Math.min(h0, h1) && h <= Math.max(h0, h1)
    }

    // Sends the painted block, unless it changes nothing.
    function commit(hours, on) {
        if (hours.some(i => cells[i] !== (on ? 2 : 0))) painted(hours, on)
    }

    Keys.onPressed: event => {
        const moves = { [Qt.Key_Left]: -1, [Qt.Key_Right]: 1, [Qt.Key_Up]: -24, [Qt.Key_Down]: 24 }
        if (event.key in moves) {
            const next = cursor + moves[event.key]
            if (next >= 0 && next < 168 && !(Math.abs(moves[event.key]) === 1 && Math.floor(next / 24) !== Math.floor(cursor / 24)))
                cursor = next
            event.accepted = true
        } else if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            commit([cursor], cells[cursor] !== 2)
            event.accepted = true
        }
    }

    // ---- Hour labels ----
    Repeater {
        model: 8
        delegate: Text {
            required property int index
            x: grid.labelWidth + index * 3 * grid.pitchX
            width: grid.pitchX
            text: String(index * 3).padStart(2, "0")
            color: Theme.muted
            font.pixelSize: 11
            font.features: { "tnum": 1 }
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // ---- Day labels ----
    Repeater {
        model: 7
        delegate: Text {
            required property int index
            y: grid.headerHeight + index * grid.pitchY
            height: grid.pitchY - grid.gap
            text: grid.days[index]
            color: Math.floor(grid.nowHour / 24) === index ? Theme.text : Theme.muted
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
        }
    }

    // ---- The hours ----
    Repeater {
        model: 168
        delegate: Rectangle {
            required property int index
            readonly property bool dragged: grid.inDrag(index)
            readonly property int value: dragged ? (grid.paintOn ? 2 : 0) : (grid.cells[index] ?? 0)
            x: grid.labelWidth + (index % 24) * grid.pitchX
            y: grid.headerHeight + Math.floor(index / 24) * grid.pitchY
            width: grid.pitchX - grid.gap
            height: grid.pitchY - grid.gap
            radius: 4
            color: value === 2 ? Theme.accent : value === 1 ? Qt.rgba(0.36, 0.88, 0.9, 0.38) : Qt.rgba(1, 1, 1, 0.045)
            opacity: dragged ? 0.8 : 1
            border.width: index === grid.nowHour || (grid.activeFocus && index === grid.cursor) || index === grid.hovered ? 1.5 : 0
            border.color: grid.activeFocus && index === grid.cursor ? Theme.accent : Theme.text
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true   // a drag paints; it doesn't scroll the page
        cursorShape: Qt.CrossCursor
        onPressed: mouse => {
            const i = grid.cellAt(mouse.x, mouse.y, false)
            if (i < 0) { mouse.accepted = false; return }
            grid.forceActiveFocus()
            grid.cursor = i
            grid.paintOn = grid.cells[i] !== 2
            grid.anchor = i
            grid.reach = i
        }
        onPositionChanged: mouse => {
            grid.hovered = grid.cellAt(mouse.x, mouse.y, false)
            if (grid.anchor >= 0) grid.reach = grid.cellAt(mouse.x, mouse.y, true)
        }
        onExited: grid.hovered = -1
        onReleased: {
            if (grid.anchor < 0) return
            const hours = []
            for (let i = 0; i < 168; ++i) if (grid.inDrag(i)) hours.push(i)
            const on = grid.paintOn
            grid.anchor = -1
            grid.reach = -1
            grid.commit(hours, on)
        }
        onCanceled: { grid.anchor = -1; grid.reach = -1 }
    }
}
