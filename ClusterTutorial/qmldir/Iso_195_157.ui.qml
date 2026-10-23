import QtQuick 2.8
import QtQuick.Layouts 1.0
import ClusterTutorial 1.0 

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
            active: Values.daytimeLights
            engineIconOffSource: "icons/dayTimeDrivingLights.png"
            engineIconOnSource: "icons/dayTimeDrivingLightsOn.png"
        }

        IsoIcon {
            id: nightTimeDimLightsIcon
            active: Values.nightTimeDimLights
            engineIconOffSource: "icons/nightTimeDimLights.png"
            engineIconOnSource: "icons/nightTimeDimLightsOn.png"
        }

        IsoIcon {
           id: nightTimeBrightLightsIcon
           active: Values.nightTimeBrightLights
           engineIconOffSource: "icons/nightTimeBrightLights.png"
           engineIconOnSource: "icons/nightTimeBrightLightsOn.png"
        }

        IsoIcon {
            id: ecuIcon
            active: Values.ecu
            engineIconOffSource: "icons/glowEcuOff.png"
            engineIconOnSource: "icons/glowEcuOn.png"
        }

        IsoIcon {
           id: glowPlugsIcon
           active: Values.glowPlugs
           engineIconOffSource: "icons/glowPlugsOff.png"
           engineIconOnSource: "icons/glowPlugsOn.png"
        }

        IsoIcon {
           id: oilSignalIcon
           active: Values.bool_oilSignal
           // Signal on for NORMAL
           engineIconOffSource: "icons/signalOil.png"
           // Signal on for any off CRITICALLY_LOW, LOW, HIGH, CRITICALLY_HIGH
           engineIconOnSource: "icons/signalOilOn.png"
        }
        
        IsoIcon {
           id: carDoorsSignalIcon
           active: Values.doorsSignal
           engineIconOffSource: "icons/carDoorsClosed.png"
           engineIconOnSource: "icons/carDoorsOpen.png"
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
