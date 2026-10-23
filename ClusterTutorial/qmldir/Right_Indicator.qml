import QtQuick

Item {
    id: root
    property bool active: false

    Image {
        anchors.centerIn: parent
        source: root.active ? "icons/rightindicatorOn.png" : "icons/rightindicator.png"
    }
}
