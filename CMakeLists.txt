#QTP0001

cmake_minimum_required(VERSION 3.21.1)
project(ClusterDashboard VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# This directory is where Qt will generate the virtual QML structure
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

# 1. Find standard Qt modules
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick) ## Core5Compat)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

# 2. Find system Protobuf and gRPC (not via Qt6)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# 3. Find Kuksa
find_package(kuksaclient REQUIRED)

# Safely copy imports ONLY if the folder exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/imports")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/imports DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

# Safely copy custom_libs ONLY if the folder exists (Desktop mode)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/custom_libs")
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/custom_libs/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()


qt_add_executable(ClusterDashboardApp main.cpp)

# 4. Define the QML Module
# The URI "ClusterTutorial" allows you to use "import ClusterTutorial" in QML
qt6_add_qml_module(ClusterDashboardApp
    URI "ClusterTutorial"
    VERSION 1.0
    NO_PLUGIN
    QML_FILES
        # Note: Paths here MUST match your 'ls -R' output exactly
        ClusterTutorial/qmldir/absIcon.ui.qml
        ClusterTutorial/qmldir/App.qml
        ClusterTutorial/qmldir/ArcArrow.qml
        ClusterTutorial/qmldir/ArcItem.qml
        ClusterTutorial/qmldir/Backgrounds_195_610.ui.qml
        ClusterTutorial/qmldir/BorderItem.qml
        ClusterTutorial/qmldir/Cherry.qml
        ClusterTutorial/qmldir/Cluster_Art.ui.qml
        ClusterTutorial/qmldir/ClusterTutorial.qml
        ClusterTutorial/qmldir/Constants.qml
        ClusterTutorial/qmldir/CustomDot.qml
        ClusterTutorial/qmldir/DirectoryFontLoader.qml
        ClusterTutorial/qmldir/EllipseItem.qml
        ClusterTutorial/qmldir/EventListModel.qml
        ClusterTutorial/qmldir/EventListSimulator.qml
        ClusterTutorial/qmldir/FlipableItem.qml
        ClusterTutorial/qmldir/Fuel_dial_195_43.ui.qml
        ClusterTutorial/qmldir/fuelIcon.ui.qml
        ClusterTutorial/qmldir/Gearbox_195_196.ui.qml
        ClusterTutorial/qmldir/GroupItem.qml
        ClusterTutorial/qmldir/iceIcon.ui.qml
        ClusterTutorial/qmldir/Iso_195_156.ui.qml
        ClusterTutorial/qmldir/Iso_195_157.ui.qml
        ClusterTutorial/qmldir/IsoIcon.ui.qml
        ClusterTutorial/qmldir/KMS_Totals_Trip.ui.qml
        ClusterTutorial/qmldir/Left_Indicator.qml
        ClusterTutorial/qmldir/Left_Indicator.ui.qml
        ClusterTutorial/qmldir/parkingBrakeIcon.ui.qml
        ClusterTutorial/qmldir/parkingLightIcon.ui.qml
        ClusterTutorial/qmldir/PieItem.qml
        ClusterTutorial/qmldir/RectangleItem.qml
        ClusterTutorial/qmldir/RegularPolygonItem.qml
        ClusterTutorial/qmldir/Right_Indicator.qml
        ClusterTutorial/qmldir/Right_Indicator.ui.qml
        ClusterTutorial/qmldir/Rpm_dial_195_83.ui.qml
        ClusterTutorial/qmldir/Speed_dial_195_151.ui.qml
        ClusterTutorial/qmldir/StarItem.qml
        ClusterTutorial/qmldir/StraightArrow.qml
        ClusterTutorial/qmldir/SvgPathItem.qml
        ClusterTutorial/qmldir/Temp_dial_195_83.ui.qml
        ClusterTutorial/qmldir/TextItem.qml
        ClusterTutorial/qmldir/TriangleItem.qml
        ClusterTutorial/qmldir/Values.qml

    RESOURCES
        ClusterTutorial/qmldir/qmldir
        ## the qmldir modules link file
        ##ClusterTutorial/qmldir/qmldir  # Add new definitions in this file
        # Adding your assets and icons to the resource system Fonts
        ClusterTutorial/qmldir/fonts/IBMPlexMono-ExtraLight.otf
        ClusterTutorial/qmldir/fonts/fonts.txt
        ClusterTutorial/qmldir/fonts/Exo2-Thin.ttf
        ClusterTutorial/qmldir/fonts/Cherry-Monospace-Light.ttf
                      
        # Adding your assets and icons to the resource system
        ClusterTutorial/qmldir/assets/arc_background_track_195_6.png
        ClusterTutorial/qmldir/assets/arc_gauge_display_195_7.png
        ClusterTutorial/qmldir/assets/arc_track_background_195_64.png
        ClusterTutorial/qmldir/assets/backgrounds_195_610.jpg
        ClusterTutorial/qmldir/assets/builtwithlogo_195_934.png
        ClusterTutorial/qmldir/assets/Cluster_Art.png
        ClusterTutorial/qmldir/assets/clustertutorial.png
        ClusterTutorial/qmldir/assets/context.jpg
        ClusterTutorial/qmldir/assets/dial_outside_ring_195_9.png
        ClusterTutorial/qmldir/assets/fuel_dial_195_43.png
        ClusterTutorial/qmldir/assets/fuell_dot_40_195_16.png
        ClusterTutorial/qmldir/assets/fuel_needle_195_27.png
        ClusterTutorial/qmldir/assets/gearbox_visual_1_195_187.png
        ClusterTutorial/qmldir/assets/gearbox_visual_195_150.png
        ClusterTutorial/qmldir/assets/gearbox_visual_2_195_185.png
        ClusterTutorial/qmldir/assets/gearbox_visual_3_195_181.png
        ClusterTutorial/qmldir/assets/gearbox_visual_4_195_179.png
        ClusterTutorial/qmldir/assets/gearbox_visual_5_195_162.png
        ClusterTutorial/qmldir/assets/iso_195_156.png
        ClusterTutorial/qmldir/assets/logo.png
        ClusterTutorial/qmldir/assets/outer_ring_195_48.png
        ClusterTutorial/qmldir/assets/outside_white_arc_195_46.png
        ClusterTutorial/qmldir/assets/outside_white_arch_195_28.png
        ClusterTutorial/qmldir/assets/rpm_arc_gauge_195_68.png
        ClusterTutorial/qmldir/assets/rpm_dial_195_83.png
        ClusterTutorial/qmldir/assets/rpm_needle_195_65.png
        ClusterTutorial/qmldir/assets/SeatService_context.svg
        ClusterTutorial/qmldir/assets/SeatService_internal.svg
        ClusterTutorial/qmldir/assets/serial-output.png
        ClusterTutorial/qmldir/assets/speed_dial_195_151.png
        ClusterTutorial/qmldir/assets/speedometer_current_kph_arc_195_1340.png
        ClusterTutorial/qmldir/assets/speedometer_needle_195_142.png
        ClusterTutorial/qmldir/assets/speedometer_needle_ring_195_90.png
        ClusterTutorial/qmldir/assets/speedometer_track_bg_195_87.png
        ClusterTutorial/qmldir/assets/sppedometer_outer_ring_195_86.png
        ClusterTutorial/qmldir/assets/temp_dial_195_83.png
        ClusterTutorial/qmldir/assets/temp_needle_195_65.png
        # (Add other critical PNGs here as needed)
        ClusterTutorial/qmldir/carLogo.png
        ClusterTutorial/qmldir/icons/absIconOn.png
        ClusterTutorial/qmldir/icons/absIcon.png
        ClusterTutorial/qmldir/icons/dayTimeDrivingLightsOn.png
        ClusterTutorial/qmldir/icons/dayTimeDrivingLights.png
        ClusterTutorial/qmldir/icons/engineIconOff.png
        ClusterTutorial/qmldir/icons/engineIconOn.png
        ClusterTutorial/qmldir/icons/fuelIconOff.png
        ClusterTutorial/qmldir/icons/fuelIconOn.png
        ClusterTutorial/qmldir/icons/iceIconOn.png
        ClusterTutorial/qmldir/icons/iceIcon.png
        ClusterTutorial/qmldir/icons/leftindicatorOn.png
        ClusterTutorial/qmldir/icons/leftindicator.png
        ClusterTutorial/qmldir/icons/nightTimeBrightLightsOn.png
        ClusterTutorial/qmldir/icons/nightTimeBrightLights.png
        ClusterTutorial/qmldir/icons/nightTimeDimLightsOn.png
        ClusterTutorial/qmldir/icons/nightTimeDimLights.png
        ClusterTutorial/qmldir/icons/parkingBrakeIconOn.png
        ClusterTutorial/qmldir/icons/parkingBrakeIcon.png
        ClusterTutorial/qmldir/icons/parkingLightIconOn.png
        ClusterTutorial/qmldir/icons/parkingLightIcon.png
        ClusterTutorial/qmldir/icons/rightindicatorOn.png
        ClusterTutorial/qmldir/icons/rightindicator.png
        ClusterTutorial/qmldir/icons/seatbeltIconOn.png
        ClusterTutorial/qmldir/icons/seatbeltIcon.png
)

# 5. Link everything to the Executable
target_link_libraries(ClusterDashboardApp PRIVATE 
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    #Qt6::Core5Compat  # Required for the Glow effect
    # Corrected System Links:
    gRPC::grpc++
    protobuf::libprotobuf
    kuksaclient       # Use the name found by find_package(kuksaclient)
)

# 6. Install logic for AGL
include(GNUInstallDirs)
install(TARGETS ClusterDashboardApp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

