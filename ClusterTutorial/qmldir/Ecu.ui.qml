import QtQuick 2.8
import QtQuick.Layouts 1.0

Item {
    id: ecu
    property alias ecuOffSource: ecuOff.source
    property alias ecuOnSource: ecuOn.source
    width: 92
    height: 64
    property bool active: false
    state: "normal"
    Image {
        id: ecuOff
        anchors.verticalCenter: parent.verticalCenter
        source: "icons/glowEcuOff.png"
        anchors.horizontalCenter: parent.horizontalCenter
        fillMode: Image.PreserveAspectFit
    }

    Image {
        id: ecuOn
        x: 0
        y: 0
        anchors.verticalCenter: parent.verticalCenter
        source: "icons/glowEcuOn.png"
        anchors.horizontalCenter: parent.horizontalCenter
        fillMode: Image.PreserveAspectFit
    }
    states: [
        State {
            name: "normal"
            when: !ecu.active

            PropertyChanges {
                target: ecuOn
                visible: false
            }

            PropertyChanges {
                target: ecuOff
                visible: true
            }

            PropertyChanges {
                target: ecu
                visible: true
            }
        },
        State {
            name: "active"
            when: ecu.active

            PropertyChanges {
                target: ecuOff
                visible: false
            }

            PropertyChanges {
                target: ecuOn
                visible: true
            }

            PropertyChanges {
                target: ecu
                visible: true
            }
        }
    ]
}

/*##^## Designer {
    D{i:0;height:61.1875;width:61.1875}
}
 ##^##*/

