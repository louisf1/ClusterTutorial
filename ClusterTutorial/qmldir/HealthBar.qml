import QtQuick 2.15
import QtQuick.Layouts 1.15 // <--- MANDATORY: This fixes the "Non-existent attached object" error

Item {
    id: root
    width: 400
    height: 50

    property string label: "STAT"
    property real value: 0       
    property color barColor: value > 90 ? "#FF0000" : (value > 70 ? "#FFA500" : "#00FF00")

    Column {
        anchors.fill: parent
        spacing: 5

        // Changed Row to RowLayout to support the Spacer
        RowLayout { 
            width: parent.width
            
            Text { 
                text: root.label
                color: "gray"
                font.pixelSize: 14
                font.bold: true 
            }

            // The Spacer: This now pushes the next item to the far right
            Item { 
                Layout.fillWidth: true 
            } 

            Text { 
                text: Math.round(root.value) + "%"
                color: "white"
                font.pixelSize: 14 
            }
        }

        // The Bar Background
        Rectangle {
            width: parent.width
            height: 12
            color: "#333333"
            radius: 6

            Rectangle {
                id: fillRect
                // Ensure the fill doesn't exceed the background width
                width: (parent.width * (Math.min(root.value, 100) / 100))
                height: parent.height
                color: root.barColor
                radius: 6

                Behavior on width {
                    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                }
            }
        }
    }
}
