/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Quick Studio Components.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import ClusterTutorial 1.0
import Qt5Compat.GraphicalEffects

Window {
    id: rootWindow
    visibility: Window.Windowed
    visible: true
    color: "black"
    flags: Qt.FramelessWindowHint | Qt.Window

    // Go fullscreen on whatever screen this window lands on —
    // fires both on initial KWin placement and after a drag-drop.
    onScreenChanged: {
        visibility = Window.FullScreen
    }

     Shortcut {
        sequence: "Space"
        onActivated: {
            view.currentIndex = (view.currentIndex + 1) % 3 
            console.log("Page Switched to: " + view.currentIndex)
        }
      }

    Item {
        id: dashboardWrapper
        width: 1920
        height: 1080
        x: 0
        y: 0
        transform: Scale {
            xScale: rootWindow.width / 1920
            yScale: rootWindow.height / 1080
            origin.x: 0
            origin.y: 0
        }
        
        SwipeView {
            id: view 	
            anchors.fill: parent
            currentIndex: 0
            interactive: false
            clip: true 

            // --- PAGE 1: The Main Dashboard ---
            Item {
                id: dashPageContainer
                width: 1920
                height: 1080
                ClusterTutorial { anchors.fill: parent }
            }
            

            // --- PAGE 2: Vehicle Info / Diagnostics ---
	    Rectangle {
		    id: infoPage2
		    width: 1920
		    height: 1080
		    color: "#1a1a1a"

	    // This function adds a new line to your terminal
	    function addEntry(msg) {
		klineModel.append({"entryText": msg});
		if (klineModel.count > 50) klineModel.remove(0); // Keep memory lean
	    } // end addEntry

	    Backgrounds_195_610 {
		x: 0
		y: 0
	    } // end Backgrounds component

	    Column {
		anchors.fill: parent
		anchors.margins: 40
		spacing: 20

		Text { 
		    text: "VEHICLE INFO AND ECU K-LINE HEALTH"
		    font.pixelSize: 40
		    color: "white" 
		} // end Page Title

		Row {
		    spacing: 20
		    StatusLed { label: "KUKSA"; active: SysHealth.kuksaStatus === "active" }
		    StatusLed { label: "BLUETOOTH"; active: SysHealth.bluetoothStatus === "active"  }
		    StatusLed { label: "ARDUINO"; active: SysHealth.arduinoStatus === "active"  }
		    StatusLed { label: "PYTHON SUPPORT SERVICE"; active: SysHealth.pythonSupportStatus === "active"  }
		} // end Status LED Row

		HealthBar { label: "CPU USAGE"; value: SysHealth.cpuPercent }
		HealthBar { label: "RAM USAGE"; value: SysHealth.ramPercent }

		Row {
		    width: parent.width
		    height: 660
		    spacing: 25

		    // 1. VEHICLE INFORMATION
		    Rectangle {
		        id: vehicledata
		        width: parent.width - (parent.width - (parent.width / 4))
		        height: 660
		        color: "transparent"
		        border.color: "#333"
		        radius: 5

		        Column {
		            anchors.fill: parent
		            anchors.margins: 15
		            spacing: 10
		        Text {
		                text: "Vehicle Information: "
		                color: "#00FF00"
		                font.bold: true
		                font.pixelSize: 24
		            } // end ECU Status Title

		            Grid {
		                columns: 2
		                spacing: 25
		                anchors.horizontalCenter: parent.horizontalCenter

		                Text { text: "VIN:"; color: "gray"; font: Constants.font }
		                Text { text: Values.vin; color: "white"; font: Constants.font }

		                Text { text: "VEHICLE:"; color: "gray"; font: Constants.font }
		                Text { text: Values.brand + " " + Values.modelName; color: "white"; font: Constants.font  }

		                Text { text: "Year Registered:"; color: "gray"; font: Constants.font  }
		                Text { text: Values.year; color: "white"; font: Constants.font  }

		                Text { text: "License Plate:"; color: "gray"; font: Constants.font  }
		                Text { text: Values.licensePlate; color: "white"; font: Constants.font  }
		                
		                Text { text: "Exterior Color:"; color: "gray"; font: Constants.font  }
		                Text { text: Values.vehicleExteriorColor; color: "white"; font: Constants.font  }

		                Text { text: "Interior Color:"; color: "gray"; font: Constants.font }
		                Text { text: Values.vehicleInteriorColor; color: "white"; font: Constants.font }

		                Text { text: "Purchase Date:"; color: "gray"; font: Constants.font }
		                Text { text: Values.purchaseDate; color: "white"; font: Constants.font }

		                Text { text: "Production Date:"; color: "gray"; font: Constants.font }
		                Text { text: Values.productionDate; color: "white"; font: Constants.font }

		                Text { text: "Known Damage:"; color: "gray"; font: Constants.font }
		                Text { text: Values.knownVehicleDamages; color: "white"; font: Constants.font }

		                Text { text: "Total Km:"; color: "gray"; font: Constants.font }
		                Text { text: Values.totalKms; color: "white"; font: Constants.font }

		                Text { text: "Trip Km:"; color: "gray"; font: Constants.font }
		                Text { text: Values.tripKms; color: "white"; font: Constants.font }
		            }//Grid with vehicle Info

		        } // // end Vehicle information Column
		    } // end // end Vehicle information Rectangle

		// 2. ECU K-LINE DIAGNOSTICS BLOCK
		Rectangle {
		    id: klineTerminal
		    width: parent.width - (parent.width - (parent.width / 4)) + 250
		    height: 660
		    color: "transparent"
		    border.color: "#333"
		    radius: 5

		    // The Bridge: Catches the 'newLogEntry' signal from SystemMonitor.cpp
		    // Moved here to the top level of the block for clean scoping
		    Connections {
			target: SysHealth
			function onNewLogEntry(message) {
			    klineModel.append({"entryText": message});
			    
			    // Auto-scroll logic: ensures the latest ECU data is always visible
			    if (klineScroll.count > 0) {
				klineScroll.positionViewAtEnd();
			    } // end scroll if
			    
			    // Keep memory lean on the Intel PC
			    if (klineModel.count > 100) {
				klineModel.remove(0);
			    } // end RAM-save if
			} // end onNewLogEntry
		    } // end Connections

		    Column {
			anchors.fill: parent
			anchors.margins: 15
			spacing: 12 // Increased spacing slightly for better readability

			Text {
			    id: terminalTitle
			    text: "ECU K-LINE STATUS: "
			    color: "#00FF00"
			    font.bold: true
			    font.pixelSize: 24
			} // end ECU Status Title

			// The Scrolling Terminal Window
			Rectangle {
			    width: parent.width
			    height: parent.height - terminalTitle.height - parent.spacing - 30 
			    color:  "transparent"
			    radius: 3
			    clip: true // ESSENTIAL: Keeps scrolling text inside the box

			    ListView {
				id: klineScroll
				anchors.fill: parent
				anchors.margins: 5
				model: klineModel
				
				delegate: Text {
				    width: klineScroll.width
				    text: "> " + model.entryText
				    color: "#00FF00"
				    font.family: "Monospace"
				    font.pixelSize: 20
				    wrapMode: Text.WrapAnywhere // Handles long hex strings correctly
				} // end Text delegate
			    } // end ListView
			} // end Terminal Inner Rectangle
		    } // end ECU K-LINE Main Column

		    ListModel { id: klineModel }

		} // end klineTerminal Main Rectangle
		} // end Side-by-Side Row
	    } // end Main Column

         }//Rectangle containing the vehicle Info

		// --- PAGE 3: System Health ---
		Rectangle {
		    id: healthPage
		    width: 1920
		    height: 1080
		    color: "#1a1a1a"

		    // These functions must live HERE (inside healthPage) to be visible to the Repeater
		function getWireColor(i) {
		    var colors = [
			"black-red",    //: Control Lever Position-Timer Control valve :Arduino AnaLog/Digital Pin  5
			"black-white",    //: ignition power on-off and Timer Control Valve :Arduino Digital Pin 33
			"black-yellow",    //: Connected to ignition key start barrel :Arduino AnaLog/Digital Pin  13
			"blue-black",    //: Timer Control Valve :Arduino AnaLog/Digital Pin  2
			"blue-pink",    //: Crankshaft Position Sensor :Arduino AnaLog/Digital Pin 1
			"brown",    //: Glow plug relay :AnaLog/Digital Digital Pin 34
			"brown-red",    //: unknown :Arduino AnaLog/Digital Pin  7
			"brown-white",    //: Control Lever Position :Arduino AnaLog/Digital Pin  9
			"brown-yellow",    //: unknown :Arduino AnaLog/Digital Pin  8
			"green",    //: FICD solenoid Valve no1 :Arduino AnaLog/Digital Pin  11
			"green-white",    //: Idle control switch :Arduino AnaLog/Digital Pin  4
			"pink",    //: FICD solenoid Valve no2 :Arduino AnaLog/Digital Pin  12
			"red",    //: standby power :Arduino Digital Pin 36
			"red-blue",    //: Control Lever Position :Arduino AnaLog/Digital Pin  6
			"red-white",    //: unknown :Arduino Digital Pin 35
			"white-blue",    //: PCM internal control :Arduino AnaLog/Digital Pin  3
			"yellow"    //: EGR solonoid Valve :Arduino AnaLog/Digital Pin 10
		    ];
		    return (i >= 0 && i < colors.length) ? colors[i] : "";
		}

		function getWireFunction(i) {
		    var funcs = [
			"Control Lever Position-Timer Control valve",    //: "black-red",    //: Control Lever Position-Timer Control valve :Arduino AnaLog/Digital Pin  5
			"ignition power on-off and Timer Control Valve",    //: "black-white",    //: ignition power on-off and Timer Control Valve :Arduino Digital Pin 33
			"Connected to ignition key start barrel",    //: "black-yellow",    //: Connected to ignition key start barrel :Arduino AnaLog/Digital Pin  13
			"Timer Control Valve",    //: "blue-black",    //: Timer Control Valve :Arduino AnaLog/Digital Pin  2
			"Crankshaft Position Sensor",    //: "blue-pink",    //: Crankshaft Position Sensor :Arduino AnaLog/Digital Pin 1
			"Glow plug relay",    //: "brown",    //: Glow plug relay :AnaLog/Digital Digital Pin 34
			"unknown",    //: "brown-red",    //: unknown :Arduino AnaLog/Digital Pin  7
			"Control Lever Position",    //: "brown-white",    //: Control Lever Position :Arduino AnaLog/Digital Pin  9
			"unknown",    //: "brown-yellow",    //: unknown :Arduino AnaLog/Digital Pin  8
			"FICD solenoid Valve no1",    //: "green",    //: FICD solenoid Valve no1 :Arduino AnaLog/Digital Pin  11
			"Idle control switch",    //: "green-white",    //: Idle control switch :Arduino AnaLog/Digital Pin  4
			"FICD solenoid Valve no2",    //: "pink",    //: FICD solenoid Valve no2 :Arduino AnaLog/Digital Pin  12
			"standby power",    //: "red",    //: standby power :Arduino Digital Pin 36
			"Control Lever Position",    //: "red-blue",    //: Control Lever Position :Arduino AnaLog/Digital Pin  6
			"unknown",    //: "red-white",    //: unknown :Arduino Digital Pin 35
			"PCM internal control",    //: "white-blue",    //: PCM internal control :Arduino AnaLog/Digital Pin  3
			"EGR solonoid Valve"    //: "yellow",    //: EGR solonoid Valve :Arduino AnaLog/Digital Pin 10
   			];
		    
		    // Safety check to prevent QML from crashing if index is out of bounds
		    return (i >= 0 && i < funcs.length) ? funcs[i] : "Unknown Pin";
		} // end getWireFunction function

		// Inside healthPage Rectangle
		function getWireHealth(i) {
		    return SysHealth.ecuWireStates[i];
		} // end getWireHealth function

		function getWireVoltage(i) {
		    var v = SysHealth.ecuWireVoltages[i];
		    if (v === undefined || v <= 0.01) return " --- ";
		    return v.toFixed(2) + "V";
		} // end getWireVoltage function

		    Backgrounds_195_610 {
			x: 0
			y: 0
		    } // end Backgrounds component

		    Column {
			anchors.fill: parent
			anchors.margins: 40
			spacing: 20

			Text { 
			    text: "SYSTEM HEALTH"
			    font.pixelSize: 40
			    color: "white" 
			} // end Page Title

			Row {
			    spacing: 20
			    StatusLed { label: "KUKSA"; active: SysHealth.kuksaStatus === "active" }
                StatusLed { label: "BLUETOOTH"; active: SysHealth.bluetoothStatus === "active"  }
		        StatusLed { label: "ARDUINO"; active: SysHealth.arduinoStatus === "active"  }
		        StatusLed { label: "PYTHON SUPPORT SERVICE"; active: SysHealth.pythonSupportStatus === "active"  }
			} // end Status LED Row

			HealthBar { label: "CPU USAGE"; value: SysHealth.cpuPercent }
			HealthBar { label: "RAM USAGE"; value: SysHealth.ramPercent }

			Row {
			    width: parent.width
			    height: 660
			    spacing: 25

			    // 1. INTEL PC BLOCK
			    Rectangle {
				id: intelBlock
				width: parent.width - (parent.width - (parent.width / 4))
				height: 660
				color: "transparent"
				border.color: "#333"
				radius: 5

				Column {
				    anchors.fill: parent
				    anchors.margins: 15
				    spacing: 10

				    Text {
				        text: "SERVICES STATUS: " + SysHealth.kuksaStatus
				        color: SysHealth.kuksaStatus === "active" ? "#00FF00" : "#FF0000"
				        font.bold: true
				        font.pixelSize: 24
				    } // end Services Status Text

				    Text {
				        text: "TOP PROCESSES:\n" + SysHealth.topProcesses
				        font.family: "Monospace"
				        color: "#00FF00"
				        font.pixelSize: 20
				        lineHeight: 1.2
				    } // end Top Processes Text
				} // end Intel Column
			    } // end intelBlock Rectangle

			    // 2. ECU DIAGNOSTICS BLOCK
			    Rectangle {
				id: ecuBlock
				width: parent.width - (parent.width - (parent.width / 4))
				height: 660
				color: "transparent"
				border.color: "#333"
				radius: 5

				Column {
				    anchors.fill: parent
				    anchors.margins: 15
				    spacing: 8

				    Text {
				        text: "ECU STATUS: "
				        color: "#00FF00"
				        font.bold: true
				        font.pixelSize: 24
				    } // end ECU Status Title

				    Column {
				        spacing: 5
				        width: parent.width

				        Repeater {
				            model: 20
				            delegate: Item {
				                width: parent.width
				                height: 25
				                visible: healthPage.getWireColor(index) !== ""

				                // Left: color dot + function description
				                Row {
				                    anchors.left: parent.left
				                    anchors.verticalCenter: parent.verticalCenter
				                    spacing: 10

				                    Rectangle {
				                        id: wireDot
				                        width: 20
				                        height: 20
				                        radius: 10
				                        border.color: "white"
				                        border.width: 1
				                        anchors.verticalCenter: parent.verticalCenter
				                        property string fullColorName: healthPage.getWireColor(index)
				                        gradient: fullColorName.indexOf("-") !== -1 ? tracerGradient : null
				                        color: fullColorName.indexOf("-") === -1 ? fullColorName : "transparent"
				                        Gradient {
				                            id: tracerGradient
				                            orientation: Gradient.Vertical
				                            GradientStop { position: 0.0; color: wireDot.fullColorName.split("-")[0] }
				                            GradientStop {
				                                position: 0.5
				                                property var parts: wireDot.fullColorName.split("-")
				                                color: parts.length > 1 ? parts[1] : parts[0]
				                            }
				                            GradientStop { position: 1.0; color: wireDot.fullColorName.split("-")[0] }
				                        }
				                    } // end wire color dot

				                    Text {
				                        text: healthPage.getWireFunction(index)
				                        color: "#AAA"
				                        font.pixelSize: 20
				                        elide: Text.ElideRight
				                        width: 280
				                        anchors.verticalCenter: parent.verticalCenter
				                    } // end function description
				                } // end left Row

				                // Right: health dot + voltage — anchored to right edge
				                Row {
				                    anchors.right: parent.right
				                    anchors.verticalCenter: parent.verticalCenter
				                    spacing: 10

				                    Rectangle {
				                        width: 12; height: 12; radius: 6
				                        color: healthPage.getWireHealth(index) ? "#00FF00" : "#FF0000"
				                        anchors.verticalCenter: parent.verticalCenter
				                    } // end health dot

				                    Text {
				                        text: healthPage.getWireVoltage(index)
				                        color: healthPage.getWireHealth(index) ? "#00FF00" : "#666666"
				                        font.family: "Monospace"
				                        font.pixelSize: 20
				                        width: 80
				                        anchors.verticalCenter: parent.verticalCenter
				                    } // end voltage text
				                } // end right Row

				            } // end Wire Entry Item
				        } // end Repeater
				    } // end Wire List Column
				} // end ECU Main Column
			    } // end ecuBlock Rectangle
			} // end Side-by-Side Row
		    } // end Main Column
		} // end healthPage Main Rectangle

        } // End SwipeView
    } // End dashboardWrapper

    // ── Drag handle ──────────────────────────────────────────────────────────
    // Sits outside dashboardWrapper so it is never scaled or clipped.
    // Click-drag it to move the window between the 7-inch and large screen;
    // onScreenChanged above resizes automatically once it lands.
    Item {
        id: dragHandle
        z: 1000
        width: 48
        height: 48
        anchors.top:    parent.top
        anchors.left:  parent.left
        anchors.topMargin:   10
        anchors.rightMargin: 10

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: "#CC0d0d1a"
            border.color: "#55FFFFFF"
            border.width: 1

            Grid {
                anchors.centerIn: parent
                columns: 3
                spacing: 5
                Repeater {
                    model: 9
                    Rectangle { width: 5; height: 5; radius: 2.5; color: "#BBBBBB" }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeAllCursor
            onPressed: {
                rootWindow.visibility = Window.Windowed
                rootWindow.startSystemMove()
            }
        }
    }
    // ─────────────────────────────────────────────────────────────────────────

} // End Window
