
cmake_minimum_required(VERSION 3.21.1)

# 2. Now you can enable C and CXX with a VERSION number
project(ClusterDashboard 
    VERSION 1.0 
    LANGUAGES C CXX
)

set(QT_NO_PRIVATE_MODULE_WARNING ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# This directory is where Qt will generate the virtual QML structure
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

# Force the compiler to treat this file as C, not C++
set_source_files_properties(wayland-agl-shell-protocol.c PROPERTIES LANGUAGE C)

# --- Dependencies ---
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickTimeline WaylandClient Core5Compat)
find_package(Qt6 COMPONENTS GuiPrivate) 
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(kuksaclient REQUIRED)
# Use PkgConfig to find Kuksa (This is already working!) 
pkg_check_modules(KUKSA REQUIRED kuksa-client)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

# 1. Find the Wayland Scanner tool automatically
find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner)

# 2. Automatically generate the C source file from the XML
#add_custom_command(
#    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/agl-shell-client-protocol.c
#    COMMAND wayland-scanner public-code < ${CMAKE_CURRENT_SOURCE_DIR}/protocol/agl-shell.xml > ${CMAKE_CURRENT_BINARY_DIR}/agl-shell-client-protocol.cpp
#    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocol/agl-shell.xml
#)

# 3. Automatically generate the C header file
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/agl-shell-client-protocol.h
    COMMAND wayland-scanner client-header < ${CMAKE_CURRENT_SOURCE_DIR}/protocol/agl-shell.xml > ${CMAKE_CURRENT_BINARY_DIR}/agl-shell-client-protocol.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocol/agl-shell.xml
)
# --- 1. Define the executable with YOUR code only ---
qt_add_executable(ClusterDashboardApp
    main.cpp
    # REMOVED: wayland-agl-shell-protocol.c (Qt handles this below)
)


qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)
cmake_policy(SET CMP0048 NEW)
# 2. Find system Protobuf and gRPC (not via Qt6)

# --- 2. Let Qt handle 100% of the Wayland generation and linking ---
# This command automatically finds the .xml, generates the .h/.c/.cpp,
# AND tells the linker to include them in ClusterDashboardApp.
qt6_generate_wayland_protocol_client_sources(ClusterDashboardApp
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/protocol/agl-shell.xml"
)

# --- 3. Link everything properly ---
target_link_libraries(ClusterDashboardApp PRIVATE 
    # Use the raw library name if the namespace target isn't found
    kuksaclient 
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Core5Compat
    Qt6::GuiPrivate
    Qt6::QuickTimeline
    Qt6::WaylandClient
    gRPC::grpc++
    protobuf::libprotobuf
)

# --- 4. Include the Private API and Build Directory ---
target_include_directories(ClusterDashboardApp PRIVATE 
    ${KUKSA_LIBRARIES}
    ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR} # <--- Essential for finding generated headers
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Studio/Application
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Components
)


# 4. Define the QML Module
# The URI "ClusterTutorial" allows you to use "import ClusterTutorial" in QML
# 5. DEFINE THE QML MODULE (Attaches QML files to the target created above)
qt6_add_qml_module(ClusterDashboardApp
    URI "ClusterTutorial"
    VERSION 1.0
    NO_PLUGIN
    

    # C++ SOURCES:
    # C++ SOURCES: Moved here from qt_add_executable

    SOURCES
        3rdparty/Studio/Application/quickstudioapplication.cpp
        3rdparty/Studio/Application/quickstudioapplication_p.h
        3rdparty/Components/qtstudiocomponentsplugin.cpp
        3rdparty/Components/QtQml/qqmlextensionplugin.h
        
    QML_FILES
        # Note: Paths here MUST match your 'ls -R' output exactly
# --- BUNDLING STUDIO MODULES AS RESOURCES ---
        # This fixes the "module not installed" errors on AGL
        3rdparty/ArcItem.qml
        3rdparty/StraightArrow.qml
        3rdparty/Components/ArcItem.qml
        3rdparty/Components/StraightArrow.qml
        3rdparty/Components/TriangleItem.qml
        3rdparty/Components/SvgPathItem.qml
        3rdparty/Components/RectangleItem.qml
        3rdparty/TriangleItem.qml
        3rdparty/SvgPathItem.qml
        3rdparty/RectangleItem.qml
        3rdparty/Studio/Components/ArcItem.qml
        3rdparty/Studio/Components/StraightArrow.qml
        3rdparty/Studio/Components/TriangleItem.qml
        3rdparty/Studio/Components/SvgPathItem.qml
        3rdparty/Studio/Components/RectangleItem.qml
        ClusterTutorial/qmldir/EventListModel.qml
        ClusterTutorial/qmldir/Cluster_Art.ui.qml
        ClusterTutorial/qmldir/EllipseItem.qml
        ClusterTutorial/qmldir/Cherry.qml
        ClusterTutorial/qmldir/FlipableItem.qml
        ClusterTutorial/qmldir/ArcItem.qml
        ClusterTutorial/qmldir/RegularPolygonItem.qml
        ClusterTutorial/qmldir/fuelIcon.ui.qml
        ClusterTutorial/qmldir/App.qml
        ClusterTutorial/qmldir/Right_Indicator.qml
        ClusterTutorial/qmldir/absIcon.ui.qml
        ClusterTutorial/qmldir/StraightArrow.qml
        ClusterTutorial/qmldir/Gearbox_195_196.ui.qml
        ClusterTutorial/qmldir/Constants.qml
        ClusterTutorial/qmldir/TextItem.qml
        ClusterTutorial/qmldir/TriangleItem.qml
        ClusterTutorial/qmldir/DirectoryFontLoader.qml
        ClusterTutorial/qmldir/Temp_dial_195_83.ui.qml
        ClusterTutorial/qmldir/StarItem.qml
        ClusterTutorial/qmldir/Left_Indicator.ui.qml
        ClusterTutorial/qmldir/Values.qml
        ClusterTutorial/qmldir/parkingBrakeIcon.ui.qml
        ClusterTutorial/qmldir/Fuel_dial_195_43.ui.qml
        ClusterTutorial/qmldir/Iso_195_156.ui.qml
        ClusterTutorial/qmldir/Right_Indicator.ui.qml
        ClusterTutorial/qmldir/IsoIcon.ui.qml
        ClusterTutorial/qmldir/Backgrounds_195_610.ui.qml
        ClusterTutorial/qmldir/Left_Indicator.qml
        ClusterTutorial/qmldir/Iso_195_157.ui.qml
        ClusterTutorial/qmldir/KMS_Totals_Trip.ui.qml
        ClusterTutorial/qmldir/Rpm_dial_195_83.ui.qml
        ClusterTutorial/qmldir/SvgPathItem.qml
        ClusterTutorial/qmldir/Speed_dial_195_151.ui.qml
        ClusterTutorial/qmldir/EventListSimulator.qml
        ClusterTutorial/qmldir/iceIcon.ui.qml
        ClusterTutorial/qmldir/GroupItem.qml
        ClusterTutorial/qmldir/BorderItem.qml
        ClusterTutorial/qmldir/RectangleItem.qml
        ClusterTutorial/qmldir/CustomDot.qml
        ClusterTutorial/qmldir/parkingLightIcon.ui.qml
        ClusterTutorial/qmldir/PieItem.qml
        ClusterTutorial/qmldir/ClusterTutorial.qml
        ClusterTutorial/qmldir/ArcArrow.qml

    RESOURCES
        3rdparty/Components/plugins.qmltypes
        3rdparty/plugins.qmltypes
        3rdparty/Studio/Components/plugins.qmltypes

       ## the qmldir modules link file
        3rdparty/Components/qmldir
        3rdparty/qmldir
        3rdparty/Studio/Components/qmldir

        ClusterTutorial/qmldir/qmldir
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
     
        protocol/agl-shell.xml
)

# This tells the compiler where to find the headers for your 3rdparty sources
target_include_directories(ClusterDashboardApp PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Studio/Application
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Components
)

# 5. --- THE VIRTUAL ALIAS (Refined) ---
# We map Values.qml to act as the qmldir for the "Data" module.
# The alias must include the "Data/" prefix to satisfy "import Data 1.0"
set_source_files_properties(ClusterTutorial/qmldir/Values.qml 
    PROPERTIES QT_RESOURCE_ALIAS "Data/qmldir"
)
# 6. Link everything to the Executable

target_link_libraries(ClusterDashboardApp PRIVATE 
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTimeline  # Required for Studio animations    Qt6::WaylandClient    gRPC::grpc++
    protobuf::libprotobuf
    kuksaclient
)

# 7. Install logic for AGL
include(GNUInstallDirs)
install(TARGETS ClusterDashboardApp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
