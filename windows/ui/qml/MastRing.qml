import QtQuick
import QtQuick.Shapes

// The mast ring: an arc for what's left, a rope-like stripe while a lock binds, and a slow glow.
// Drawn by the GPU (Qt Quick Shapes); only the arc length and the glow's opacity change.
Item {
    id: ring

    property real progress: 0      // fraction left, 0..1
    property bool bound: false     // a lock holds
    property color tint: Theme.accent
    property real thickness: 12
    property bool animated: true
    default property alias content: centre.data

    implicitWidth: 260
    implicitHeight: 260
    readonly property real radius: Math.min(width, height) / 2 - thickness - 6

    Behavior on progress {
        enabled: ring.animated
        NumberAnimation { duration: 900; easing.type: Easing.OutCubic }
    }

    Rectangle {  // glow
        anchors.centerIn: parent
        width: 2 * ring.radius + 2 * ring.thickness
        height: width
        radius: width / 2
        color: "transparent"
        border.width: ring.thickness * 2.2
        border.color: Qt.rgba(ring.tint.r, ring.tint.g, ring.tint.b, 0.09)
        visible: ring.bound

        SequentialAnimation on opacity {
            running: ring.animated && ring.bound
            loops: Animation.Infinite
            NumberAnimation { to: 0.35; duration: 3000; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1; duration: 3000; easing.type: Easing.InOutSine }
        }
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {  // track
            fillColor: "transparent"
            strokeColor: Qt.rgba(ring.tint.r, ring.tint.g, ring.tint.b, 0.14)
            strokeWidth: ring.thickness
            PathAngleArc {
                centerX: ring.width / 2; centerY: ring.height / 2
                radiusX: ring.radius; radiusY: ring.radius
                startAngle: 0; sweepAngle: 360
            }
        }
        ShapePath {  // what's left
            fillColor: "transparent"
            strokeColor: ring.progress > 0 ? ring.tint : "transparent"
            strokeWidth: ring.thickness
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.width / 2; centerY: ring.height / 2
                radiusX: ring.radius; radiusY: ring.radius
                startAngle: -90; sweepAngle: 360 * ring.progress
            }
        }
    }

    // The rope: short dark ticks across the arc while a lock binds. Plain rectangles, so it looks the
    // same on every renderer.
    Repeater {
        model: ring.bound ? 72 : 0
        delegate: Rectangle {
            required property int index
            readonly property real angle: (index + 0.5) / 72 * 2 * Math.PI - Math.PI / 2
            visible: (index + 1) / 72 <= ring.progress
            width: 3
            height: ring.thickness
            color: Qt.rgba(4 / 255, 10 / 255, 22 / 255, 0.55)
            x: ring.width / 2 + ring.radius * Math.cos(angle) - width / 2
            y: ring.height / 2 + ring.radius * Math.sin(angle) - height / 2
            rotation: angle * 180 / Math.PI + 90
        }
    }

    Item {
        id: centre
        anchors.centerIn: parent
        width: childrenRect.width
        height: childrenRect.height
    }
}
