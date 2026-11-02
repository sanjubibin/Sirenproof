import QtQuick
import QtQuick.Layouts

// A panel of the HUD: content stacks top to bottom inside.
Rectangle {
    id: card

    default property alias content: column.data
    property alias spacing: column.spacing
    property int padding: 22

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.edge
    implicitWidth: column.implicitWidth + 2 * padding
    implicitHeight: column.implicitHeight + 2 * padding

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: card.padding
        spacing: 14
    }
}
