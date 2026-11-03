import QtQuick
import QtQuick.Controls.Basic

// A pill button. kind: "primary" (filled sea glass), "plain" (outlined) or "amber" (waiting things).
Button {
    id: control

    property string kind: "plain"

    implicitHeight: 44
    leftPadding: 20
    rightPadding: 20
    hoverEnabled: true
    opacity: enabled ? 1 : 0.45
    font.family: Theme.body
    font.pixelSize: 15
    font.weight: Font.DemiBold

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.kind === "primary" ? Theme.inkOnAccent : control.kind === "amber" ? Theme.amber : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: height / 2
        color: control.kind === "primary" ? Theme.accent : "transparent"
        border.color: control.kind === "primary" ? Theme.accent : control.kind === "amber" ? Theme.amberEdge : Theme.edgeStrong

        Rectangle {  // keyboard focus ring
            anchors.fill: parent
            anchors.margins: -5
            radius: height / 2
            color: "transparent"
            border.width: 3
            border.color: Theme.accent
            visible: control.visualFocus
        }
    }
}
