

/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Design Studio.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/
import QtQuick 2.8
import QtQuick.Timeline 1.0
import Data 1.0 as Data
import QtQuick.Studio.Components 1.0
import QtQuick.Studio.Effects 1.0

Item {
    id: cluster_Art
    width: 1720
    height: 700

    Image {
        id: cluster_ArtAsset
        x: 0
        y: 0
        source: "assets/Cluster_Art.png"
    }

    Backgrounds_195_610 {
        x: 0
        y: 0
    }

    Item {
        id: isoIconsEffect
        x: 510
        y: 860
        width: 920
        height: 142

        Iso_195_156 {
            id: isoIcons
            x: 0
            y: 37
        }
    }

/* ---Lights--- iso_195_157 */
    Item {
        id: headlightsInstance
        x: 510
        y: 750
        width: 920
        height: 142

        Iso_195_157 {
            id: isoIconsLights
            x: 0
            y: 37
        }
    }
//----------------------------

    Gearbox_195_196 {
        id: gearbox_195_196
        x: 928
        y: 571
        currentGear: Data.Values.currentGear
    }


// Your new Temperature Gauge
    Temp_dial_195_83 {
        id: temp_dial_195_83
        
        // Positioning: Top-Left corner
        x: 290  // Adds a small 60-pixel margin from the left edge
        y: 160  // Adds a small 60-pixel margin from the top edge
        
        // Scaling: Shrink it so it acts as a secondary corner gauge
        // 0.5 means 50% of its original size. Adjust this until it looks right!
        scale: 0.5 
        transformOrigin: Item.TopLeft // Ensures it scales towards the corner, not the center
        
        // --- ADD THESE TWO LINES HERE ---
        displayTemp: Data.Values.displayTemp
        tempFrame: (Data.Values.temp - 40) * 10
    }
        
/* --- Left Indicator --- */
    Left_Indicator {
        id: leftIndicatorInstance
        x: 540
        y: 497
        width: 100 
        height: 100
        active: Data.Values.leftindicator // Must match Values.qml
    }

    /* --- Right Indicator --- */
    Right_Indicator {
        id: rightIndicatorInstance
        x: 1300
        y: 497
        width: 100
        height: 100
        active: Data.Values.rightindicator // Must match Values.qml
    }
        
    KMS_Totals_Trip {
        id: totaltrips
        x: 700
        y: 480
        width: 512
        height: 630
        readout_total_kms: Data.Values.totalKms.toFixed(0)
        readout_total_trip: Data.Values.tripKms.toFixed(1)
    }
    
    Fuel_dial_195_43 {
        id: fuelDial
        x: 1453
        y: 450
        width: 512
        height: 630
        rangeDisplay: Data.Values.displayRange
        litersDisplay: Data.Values.displayLiters
        fuelFrame: Data.Values.liters
    }

    Rpm_dial_195_83 {
        id: rpmDial
        width: 508
        height: 591
        x: 69
        y: 450
        displayRpm: Data.Values.displayRpm
        rpmFrame: Data.Values.rpm
    }

    FlipableItem {
        id: flipable
        x: 566
        y: 8
        width: 778
        height: 730
        opacity: 1

        Speed_dial_195_151 {
            x: 95
            y: 91
            kplDisplay: Data.Values.displayKpl
            kphDisplay: Data.Values.displayKph
            kphFrame: Data.Values.kph
        }

        Image {
            id: image
            x: 181
            y: 240
            source: "carLogo.png"
        }
    }

    Timeline {
        id: bootTImeline
        animations: [
            TimelineAnimation {
                id: timelineAnimation
                property: "currentFrame"
                duration: 5000
                running: false
                loops: 1
                to: 5000
                from: 0
            }
        ]
        startFrame: 0
        endFrame: 5000
        enabled: true

// ----------------------------------------------------------
// -------------- All the lights bar stuff start-------------
// ----------------------------------------------------------
// ----------------------------------------------------------
       // --- ADDED: Headlights Animation ---
        KeyframeGroup {
            target: isoIconsLights
            property: "opacity"

            Keyframe {
                value: 0
                frame: 0
            }

            Keyframe {
                value: 0
                frame: 2977 // Wait in the dark
            }

            Keyframe {
                easing.bezierCurve: [0.07, 0.82, 0.17, 1.00, 1, 1]
                value: 1
                frame: 3362 // Fade in with the other ISO icons
            }
        }
        
        // --- ADDED: Headlights Animation ---
        KeyframeGroup {
            target: leftIndicatorInstance
            property: "opacity"

            Keyframe {
                value: 0
                frame: 0
            }

            Keyframe {
                value: 0
                frame: 2977 // Wait in the dark
            }

            Keyframe {
                easing.bezierCurve: [0.07, 0.82, 0.17, 1.00, 1, 1]
                value: 1
                frame: 3362 // Fade in with the other ISO icons
            }
        }

        KeyframeGroup {
            target: rightIndicatorInstance
            property: "scale"

            Keyframe {
                value: 0.01
                frame: 0
            }

            Keyframe {
                value: 0.01
                frame: 2977 // Stay tiny
            }

            Keyframe {
                easing.bezierCurve: [0.07, 0.82, 0.17, 1.00, 1, 1]
                value: 1
                frame: 3473 // Pop out to full size
            }
        }

        KeyframeGroup {
            target: totaltrips
            property: "scale"

            Keyframe {
                value: 0.01
                frame: 0
            }

            Keyframe {
                value: 0.01
                frame: 2977 // Stay tiny
            }

            Keyframe {
                easing.bezierCurve: [0.07, 0.82, 0.17, 1.00, 1, 1]
                value: 1
                frame: 3473 // Pop out to full size
            }
        } //totaltrips
// -------------- All the lights bar stuff end --------------
// ----------------------------------------------------------
// ----------------------------------------------------------
        KeyframeGroup {
            target: flipable
            property: "flipAngle"
            Keyframe {
                value: 180
                frame: 0
            }

            Keyframe {
                value: 180
                frame: 2389
            }

            Keyframe {
                easing.bezierCurve: [0.90, 0.03, 0.69, 0.22, 1, 1]
                value: 1.1
                frame: 4117
            }
        }

        KeyframeGroup {
            target: flipable
            property: "opacity"
            Keyframe {
                value: 0
                frame: 0
            }

            Keyframe {
                easing.bezierCurve: [0.17, 0.84, 0.44, 1.00, 1, 1]
                value: 1
                frame: 1015
            }
        }

        KeyframeGroup {
            target: gearbox_195_196
            property: "opacity"

            Keyframe {
                value: 0
                frame: 0
            }

            Keyframe {
                value: 0
                frame: 3616
            }
        }

// --- ADDED: Temperature Gauge Opacity Fade-In ---
        KeyframeGroup {
            target: temp_dial_195_83
            property: "opacity"

            Keyframe {
                value: 0
                frame: 0
            }

            Keyframe {
                value: 0
                frame: 2386 // Wait in the dark alongside the RPM dial
            }

            Keyframe {
                easing.bezierCurve: [0.95, 0.05, 0.80, 0.04, 1, 1]
                value: 1
                frame: 3299 // Fade to full visibility
            }
        }

        // --- ADDED: Temperature Gauge "Swing/Zoom" Effect ---
        KeyframeGroup {
            target: temp_dial_195_83
            property: "scale"
            
            Keyframe {
                value: 0.01
                frame: 5
            }

            Keyframe {
                value: 0.01
                frame: 2391 // Stay tiny alongside the RPM dial
            }

            Keyframe {
                easing.bezierCurve: [0.95, 0.05, 0.80, 0.04, 1, 1]
                value: 0.5 // CRITICAL: Stop at 0.5 so it stays a small corner gauge!
                frame: 3294
            }
        }


        KeyframeGroup {
            target: rpmDial
            property: "opacity"

            Keyframe {
                value: 0
                frame: 0
            }

            Keyframe {
                value: 0
                frame: 2386
            }

            Keyframe {
                easing.bezierCurve: [0.95, 0.05, 0.80, 0.04, 1, 1]
                value: 1
                frame: 3299
            }
        }

        KeyframeGroup {
            target: rpmDial
            property: "scale"
            Keyframe {
                value: 0.01
                frame: 5
            }

            Keyframe {
                value: 0.01
                frame: 2391
            }

            Keyframe {
                easing.bezierCurve: [0.95, 0.05, 0.80, 0.04, 1, 1]
                value: 1
                frame: 3294
            }
        }

        KeyframeGroup {
            target: fuelDial
            property: "opacity"
            Keyframe {
                frame: 0
                value: 0
            }

            Keyframe {
                frame: 2386
                value: 0
            }

            Keyframe {
                frame: 3299
                value: 1
                easing.bezierCurve: [0.95, 0.05, 0.80, 0.04, 1, 1]
            }
        }

        KeyframeGroup {
            target: fuelDial
            property: "scale"
            Keyframe {
                frame: 5
                value: 0.01
            }

            Keyframe {
                frame: 2391
                value: 0.01
            }

            Keyframe {
                frame: 3294
                value: 1
                easing.bezierCurve: [0.95, 0.05, 0.80, 0.04, 1, 1]
            }
        }

        KeyframeGroup {
            target: isoIconsEffect
            property: "scale"

            Keyframe {
                value: 0.01
                frame: 0
            }

            Keyframe {
                value: 0.01
                frame: 2977
            }

            Keyframe {
                easing.bezierCurve: [0.07, 0.82, 0.17, 1.00, 1, 1]
                value: 1
                frame: 3473
            }
        }

        KeyframeGroup {
            target: isoIconsEffect
            property: "opacity"

            Keyframe {
                value: 0
                frame: 2977
            }

            Keyframe {
                easing.bezierCurve: [0.07, 0.82, 0.17, 1.00, 1, 1]
                value: 1
                frame: 3362
            }

            Keyframe {
                value: 0
                frame: 0
            }
        }
    }

    states: [
        State {
            name: "bootState"
            when: Data.Values.booting

            PropertyChanges {
                target: bootTImeline
                enabled: true
            }

            PropertyChanges {
                target: timelineAnimation
                running: true
            }
        },
        State {
            name: "running"
            when: !Data.Values.booting

            PropertyChanges {
                target: gearbox_195_196
                opacity: 1
            }

            PropertyChanges {
                target: bootTImeline
                currentFrame: 5000
                enabled: true
            }
        }
    ]
}
