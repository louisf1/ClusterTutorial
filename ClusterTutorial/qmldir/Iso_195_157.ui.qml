import QtQuick 2.8
import QtQuick.Layouts 1.0
import Data 1.0 as Data

Item {
    id: iso_195_157
    width: 900
    height: 68

    Image {
        id: iso_195_157Asset
        x: 8
        y: -3
        width: 888
        height: 90
        source: "assets/iso_195_156.png"
    }

    RowLayout {
        x: 45
        y: 10
        scale: 0.9
        spacing: 40

        IsoIcon {
            id: daytimeLightsIcon
            active: Data.Values.daytimeLights
            engineIconOffSource: "icons/dayTimeDrivingLights.png"
            engineIconOnSource: "icons/dayTimeDrivingLightsOn.png"
        }

        IsoIcon {
            id: nightTimeDimLightsIcon
            active: Data.Values.nightTimeDimLights
            engineIconOffSource: "icons/nightTimeDimLights.png"
            engineIconOnSource: "icons/nightTimeDimLightsOn.png"
        }

        IsoIcon {
           id: nightTimeBrightLightsIcon
           active: Data.Values.nightTimeBrightLights
           engineIconOffSource: "icons/nightTimeBrightLights.png"
           engineIconOnSource: "icons/nightTimeBrightLightsOn.png"
        }
    }
}

/*##^## Designer {
    D{i:0;width:900;height:68}
    D{i:3;active__AT__NodeInstance:false}
    D{i:4;active__AT__NodeInstance:false}
    D{i:5;active__AT__NodeInstance:false}
}
 ##^##*/
