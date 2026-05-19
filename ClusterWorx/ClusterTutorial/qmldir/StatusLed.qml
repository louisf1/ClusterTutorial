// StatusLed.qml
import QtQuick 2.15

Item {
    id: root
    width: 250 // Increased slightly to fit longer labels
    height: 40

    property string label: "SERVICE"
    property bool active: false

    Row {
        anchors.fill: parent
        spacing: 15
        // REMOVED: verticalAlignment: Text.AlignVCenter (This was the error)

        Rectangle {
            width: 18
            height: 18
            radius: 9
            color: root.active ? "#00FF00" : "#FF0000"
            anchors.verticalCenter: parent.verticalCenter // Correct way to center in Row
            
            border.color: Qt.darker(color, 1.5)
            border.width: 2
        }

        Text {
            text: root.label
            color: "white"
            font.pixelSize: 16
            font.bold: true
            font.family: "Monospace"
            height: parent.height
            verticalAlignment: Text.AlignVCenter // This is where this property belongs
        }
    }
}
