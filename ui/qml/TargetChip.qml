import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// A site, page or app as a pill, with × to take it out.
Rectangle {
    id: chip

    required property string target
    property string removeName: `Remove ${target}`
    signal removed()

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
        Text { text: chip.target; color: Theme.text; font.pixelSize: 14 }
        Button {
            id: remove
            implicitWidth: 28; implicitHeight: 28
            hoverEnabled: true
            Accessible.name: chip.removeName
            onClicked: chip.removed()
            contentItem: Text { text: "×"; color: Theme.muted; font.pixelSize: 16; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
            background: Rectangle {
                radius: 14
                color: remove.hovered ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                border.width: remove.visualFocus ? 2 : 0
                border.color: Theme.accent
            }
        }
    }
}
