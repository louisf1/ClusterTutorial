import QtQuick

Item {
    id: root
    property bool active: false
    property bool blinkVisible: false

    Timer {
        interval: 500
        repeat: true
        running: root.active
        onTriggered: root.blinkVisible = !root.blinkVisible
        onRunningChanged: if (!running) root.blinkVisible = false
    }

    Image {
        anchors.centerIn: parent
        source: (root.active && root.blinkVisible) ? "icons/rightindicatorOn.png" : "icons/rightindicator.png"
    }
}
