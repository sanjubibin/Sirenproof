import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// One option of several, as a tile with a title and a line of detail. Lit while chosen.
Button {
    id: choice

    property string detail
    property bool chosen

    Layout.fillWidth: true
    Layout.fillHeight: true  // as tall as the tallest in its row, so a row of tiles lines up
    padding: 14
    hoverEnabled: true
    Accessible.checkable: true
    Accessible.checked: chosen

    contentItem: ColumnLayout {
        spacing: 2
        Text { text: choice.text; color: Theme.text; font.weight: Font.DemiBold }
        Text {
            Layout.fillWidth: true
            text: choice.detail
            color: Theme.muted
            font.pixelSize: 13
            visible: text.length > 0
            wrapMode: Text.Wrap
        }
        Item { Layout.fillHeight: true }  // text at the top of a taller tile
    }
    background: Rectangle {
        radius: 14
        color: choice.chosen ? Theme.accentSoft : choice.hovered ? Qt.rgba(1, 1, 1, 0.03) : "transparent"
        border.color: choice.chosen || choice.visualFocus ? Theme.accent : Theme.edge
        border.width: choice.visualFocus ? 2 : 1
    }
}
