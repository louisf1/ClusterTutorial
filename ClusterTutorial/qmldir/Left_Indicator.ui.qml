import QtQuick 2.8
import QtQuick.Layouts 1.0

Item {
    id: leftindicator
    property alias leftindicatorOffSource: leftindicatorOff.source
    property alias leftindicatorOnSource: leftindicatorOn.source
    width: 92
    height: 64
    property bool active: false
    state: "normal"
    Image {
        id: leftindicatorOff
        anchors.verticalCenter: parent.verticalCenter
        source: "icons/leftindicator.png"
        anchors.horizontalCenter: parent.horizontalCenter
        fillMode: Image.PreserveAspectFit
    }

    Image {
        id: leftindicatorOn
        x: 0
        y: 0
        anchors.verticalCenter: parent.verticalCenter
        source: "icons/leftindicatorOn.png"
        anchors.horizontalCenter: parent.horizontalCenter
        fillMode: Image.PreserveAspectFit
    }
    states: [
        State {
            name: "normal"
            when: !leftindicator.active

            PropertyChanges {
                target: leftindicatorOn
                visible: false
            }

            PropertyChanges {
                target: leftindicatorOff
                visible: true
            }

            PropertyChanges {
                target: leftindicator
                visible: true
            }
        },
        State {
            name: "active"
            when: leftindicator.active

            PropertyChanges {
                target: leftindicatorOff
                visible: false
            }

            PropertyChanges {
                target: leftindicatorOn
                visible: true
            }

            PropertyChanges {
                target: leftindicator
                visible: true
            }
        }
    ]
}

/*##^## Designer {
    D{i:0;height:61.1875;width:61.1875}
}
 ##^##*/

