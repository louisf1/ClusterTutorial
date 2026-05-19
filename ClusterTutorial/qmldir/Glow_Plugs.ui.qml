import QtQuick 2.8
import QtQuick.Layouts 1.0

Item {
    id: glowplugs
    property alias glowplugsOffSource: glowplugsOff.source
    property alias glowplugsOnSource: glowplugsOn.source
    width: 92
    height: 64
    property bool active: false
    state: "normal"
    Image {
        id: glowplugsOff
        anchors.verticalCenter: parent.verticalCenter
        source: "icons/glowPlugsOff.png"
        anchors.horizontalCenter: parent.horizontalCenter
        fillMode: Image.PreserveAspectFit
    }

    Image {
        id: glowplugsOn
        x: 0
        y: 0
        anchors.verticalCenter: parent.verticalCenter
        source: "icons/glowPlugsOn.png"
        anchors.horizontalCenter: parent.horizontalCenter
        fillMode: Image.PreserveAspectFit
    }
    states: [
        State {
            name: "normal"
            when: !glowplugs.active

            PropertyChanges {
                target: glowplugsOn
                visible: false
            }

            PropertyChanges {
                target: glowplugsOff
                visible: true
            }

            PropertyChanges {
                target: glowplugs
                visible: true
            }
        },
        State {
            name: "active"
            when: glowplugs.active

            PropertyChanges {
                target: glowplugsOff
                visible: false
            }

            PropertyChanges {
                target: glowplugsOn
                visible: true
            }

            PropertyChanges {
                target: glowplugs
                visible: true
            }
        }
    ]
}

/*##^## Designer {
    D{i:0;height:61.1875;width:61.1875}
}
 ##^##*/

