import QtQuick

Item {
    id: root
    property bool active: false

    Image {
        anchors.centerIn: parent
        // Automatically swaps the image based on the 'active' property
        source: root.active ? "icons/leftindicatorOn.png" : "icons/leftindicator.png"
    }
}
