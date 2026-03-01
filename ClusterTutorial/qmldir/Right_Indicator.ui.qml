import QtQuick 2.8
import QtQuick.Layouts 1.0

Item {
    id: rightindicator
    // Use the generic 'active' property your Cluster_Art is looking for
    property bool active: false 
    
    // Standardize these aliases so they match your other icons
    property alias engineIconOffSource: rightindicatorOff.source
    property alias engineIconOnSource: rightindicatorOn.source
    
    width: 92
    height: 64

    Image {
        id: rightindicatorOff
        anchors.centerIn: parent
        source: "icons/rightindicator.png"
        fillMode: Image.PreserveAspectFit
        visible: !rightindicator.active // Simplified toggle
    }

    Image {
        id: rightindicatorOn
        anchors.centerIn: parent
        source: "icons/rightindicatorOn.png"
        fillMode: Image.PreserveAspectFit
        visible: rightindicator.active // Simplified toggle
    }
}
