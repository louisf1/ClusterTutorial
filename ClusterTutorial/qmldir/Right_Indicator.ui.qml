import QtQuick 2.8
import QtQuick.Layouts 1.0

Item {
    id: rightindicator
    property alias rightindicatorOffSource: rightindicatorOff.source
    property alias rightindicatorOnSource: rightindicatorOn.source
    width: 92
    height: 64
    property bool active: false
    state: "normal"
    Image {
        id: rightindicatorOff
        anchors.verticalCenter: parent.verticalCenter
        source: "icons/rightindicator.png"
        anchors.horizontalCenter: parent.horizontalCenter
        fillMode: Image.PreserveAspectFit
    }

    Image {
        id: rightindicatorOn
        x: 0
        y: 0
        anchors.verticalCenter: parent.verticalCenter
        source: "icons/rightindicatorOn.png"
        anchors.horizontalCenter: parent.horizontalCenter
        fillMode: Image.PreserveAspectFit
    }
    states: [
        State {
            name: "normal"
            when: !rightindicator.active

            PropertyChanges {
                target: rightindicatorOn
                visible: false
            }

            PropertyChanges {
                target: rightindicatorOff
                visible: true
            }

            PropertyChanges {
                target: rightindicator
                visible: true
            }
        },
        State {
            name: "active"
            when: rightindicator.active

            PropertyChanges {
                target: rightindicatorOff
                visible: false
            }

            PropertyChanges {
                target: rightindicatorOn
                visible: true
            }

            PropertyChanges {
                target: rightindicator
                visible: true
            }
        }
    ]
}

/*##^## Designer {
    D{i:0;height:61.1875;width:61.1875}
}
 ##^##*/

