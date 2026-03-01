/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
****************************************************************************/
import QtQuick 2.8
import QtQuick.Layouts 1.0
import Data 1.0 as Data

Item {
    id: total_kms_completed

    property alias readout_total_kms: readout_total_kms.text
    property alias readout_total_trip: readout_total_trip.text
    
    width: 100
    height: 100
    x: 0 // Move back into visible range
    y: 0 

    Item {
        id: total_km_readout


        Text {
            id: odo
            x: -104
            y: 220
            width: 170
            height: 28
            color: "#ffffff" // Standard 6-digit hex for green
            // Bind to a new property in Values.qml (see step 4)
            text: "ODO TRIP:  " 
            font.pixelSize: 20
            font.family: "Cherry"
            horizontalAlignment: Text.AlignHCenter
        }
        
        Text {
            id: readout_total_kms
            x: -4
            y: 220
            width: 170
            height: 28
            color: "#9ff516" // Standard 6-digit hex for green
            // Bind to a new property in Values.qml (see step 4)
            text: Data.Values.totalKms 
            font.pixelSize: 20
            font.family: "Cherry"
            horizontalAlignment: Text.AlignHCenter
        }

       Text {
            id: trip
            x: 350
            y: 220
            width: 170
            height: 28
            color: "#ffffff" // Standard 6-digit hex for green
            // Bind to a new property in Values.qml (see step 4)
            text: "TRIP:  " 
            font.pixelSize: 20
            font.family: "Cherry"
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            id: readout_total_trip
            x: 422
            y: 220
            width: 170
            height: 28 // Offset so they don't overlap
            color: "#9ff516"
            text: Data.Values.tripKms
            font.pixelSize: 20
            font.family: "Cherry"
            horizontalAlignment: Text.AlignHCenter
        }
    }
}

