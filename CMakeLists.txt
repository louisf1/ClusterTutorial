# =============================================================================
# CMakeLists.txt — ClusterDashboard
# Fixes applied (see inline comments tagged [FIX]):
#   - cmake_policy(CMP0048) moved before project()
#   - Consolidated all find_package Qt6 calls into one
#   - Removed duplicate find_package(Qt6 Core5Compat)
#   - Removed pkg_check_modules(KUKSA) — find_package(kuksaclient) is sufficient
#   - Removed dead set_source_files_properties for wayland-agl-shell-protocol.c
#   - Removed orphaned add_custom_command for agl-shell-client-protocol.h
#   - Removed NO_PLUGIN — C++ sources register QML types
#   - Fixed target_include_directories: replaced ${KUKSA_LIBRARIES} with correct dirs
#   - Removed broken QT_RESOURCE_ALIAS for Values.qml / Constants.qml
#   - Merged all target_link_libraries into one consolidated block
#   - Removed second redundant target_include_directories block
#   - Qt prefix path pinned to $HOME/Qt/6.10.3/gcc_64
# =============================================================================

# [FIX] CMP0048 must be set BEFORE project() to have any effect
cmake_minimum_required(VERSION 3.18.4)
cmake_policy(SET CMP0048 NEW)

# [FIX] Qt comes from Debian bookworm packages (qt6-base-dev 6.4.2, installed
#       by install_dependencies.sh) in standard system paths — no prefix pin
#       needed. The previous pin to ~/Qt/6.10.3/gcc_64 was the x86-64 build,
#       unusable on this aarch64 board; the official arm64 builds (6.8/6.10)
#       crash or need glibc >=2.38, newer than this Debian 11 vendor image can
#       provide. Override on the command line if needed:
#       cmake -DCMAKE_PREFIX_PATH=... ..

project(ClusterDashboard
    VERSION 1.0
    LANGUAGES C CXX
)

# [FIX] Qt policies must come after project()
#qt_policy(SET QTP0001 NEW)
#qt_policy(SET QTP0004 NEW)

set(QT_NO_PRIVATE_MODULE_WARNING ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# This directory is where Qt will generate the virtual QML structure
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

# =============================================================================
# Dependencies
# =============================================================================

# [FIX] All Qt6 components in a single find_package call — removed duplicate
#       Core5Compat request and merged GuiPrivate (OPTIONAL so it doesn't hard-fail
#       on systems that lack it, matching the original intent)
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickTimeline
    WaylandClient
    Core5Compat
)
find_package(Qt6 OPTIONAL_COMPONENTS GuiPrivate)

# [FIX] Debian's libgrpc-dev ships pkg-config files but no CMake config
#       package, so find_package(gRPC) needs our own module (cmake/FindgRPC.cmake).
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# [FIX] kuksaclient installs a proper CMake config
#       (/usr/local/lib/cmake/kuksaclient/kuksaclientConfig.cmake).
#       find_package is the correct and only mechanism needed.
#       pkg_check_modules(KUKSA ...) has been removed — it was finding the same
#       library via a .pc file and producing incorrect ${KUKSA_LIBRARIES} that
#       were being passed to target_include_directories by mistake.
find_package(kuksaclient REQUIRED)

# Wayland scanner tool (used by qt6_generate_wayland_protocol_client_sources)
find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner REQUIRED)

# [FIX] Removed:
#   - set_source_files_properties(wayland-agl-shell-protocol.c ...) — file not in any target
#   - add_custom_command for agl-shell-client-protocol.h — orphaned, unreachable,
#     and already superseded by qt6_generate_wayland_protocol_client_sources below

# =============================================================================
# Executable target
# =============================================================================

qt_add_executable(ClusterDashboardApp
    main.cpp
    SystemMonitor.cpp
    SystemMonitor.h
)

# Let Qt generate Wayland protocol glue (header + source) from the XML and
# attach it to the target automatically — this replaces the old manual
# wayland-scanner custom commands entirely.
qt6_generate_wayland_protocol_client_sources(ClusterDashboardApp
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/protocol/agl-shell.xml"
)

# =============================================================================
# Include directories  (single consolidated block)
# =============================================================================

# [FIX] Removed ${KUKSA_LIBRARIES} — that variable holds linker flags, not
#       include paths, and belonged to the now-removed pkg_check_modules call.
#       The kuksaclient imported target (from find_package) carries its own
#       include directories automatically via target_link_libraries below.
target_include_directories(ClusterDashboardApp PRIVATE
    ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}                              # generated Wayland headers land here
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Studio/Application
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Components
)

# =============================================================================
# Link libraries  (single consolidated block)
# =============================================================================

# [FIX] Merged the two separate target_link_libraries calls into one.
#       The second original block had Qt6::WaylandClient and gRPC::grpc++
#       accidentally inside a comment — they are restored here as real entries.
target_link_libraries(ClusterDashboardApp PRIVATE
    kuksaclient
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Core5Compat
    Qt6::QuickTimeline
    Qt6::WaylandClient
    gRPC::grpc++
    protobuf::libprotobuf
)

# GuiPrivate is optional — only link if it was found
if(TARGET Qt6::GuiPrivate)
    target_link_libraries(ClusterDashboardApp PRIVATE Qt6::GuiPrivate)
endif()

# =============================================================================
# QML Module
# =============================================================================

# [FIX] Removed NO_PLUGIN.
#       quickstudioapplication.cpp and qtstudiocomponentsplugin.cpp register
#       QML types. Suppressing the plugin with NO_PLUGIN would prevent those
#       registrations from being called at runtime, silently breaking QML type
#       resolution. Qt will generate and statically back the plugin correctly.
#
# [FIX] Removed the broken QT_RESOURCE_ALIAS block that mapped both Values.qml
#       AND Constants.qml to the same path "Data/qmldir" — only one file can
#       occupy a path, and neither file is a qmldir anyway (they are pragma
#       Singleton QML objects). Both files remain listed normally in QML_FILES
#       below and are declared as singletons in ClusterTutorial/qmldir/qmldir.
#       Import them in QML as:
#           import ClusterTutorial
#           Constants.width   // works
#           Values.kph        // works
#       Ensure ClusterTutorial/qmldir/qmldir contains:
#           singleton Constants 1.0 Constants.qml
#           singleton Values    1.0 Values.qml

qt6_add_qml_module(ClusterDashboardApp
    URI "ClusterTutorial"
    VERSION 1.0

    SOURCES
        3rdparty/Studio/Application/quickstudioapplication.cpp
        3rdparty/Studio/Application/quickstudioapplication_p.h
        3rdparty/Components/qtstudiocomponentsplugin.cpp
        3rdparty/Components/QtQml/qqmlextensionplugin.h

    QML_FILES
        # --- 3rdparty Studio component bundles ---
        # Bundled here to fix "module not installed" errors on AGL
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

        # --- ClusterTutorial application QML ---
        ClusterTutorial/qmldir/EventListModel.qml
        ClusterTutorial/qmldir/Ecu.qml
        ClusterTutorial/qmldir/Ecu.ui.qml
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
        # [FIX] Constants.qml and Values.qml remain as normal QML_FILES.
        #       They are pragma Singleton — declare them in
        #       ClusterTutorial/qmldir/qmldir as:
        #           singleton Constants 1.0 Constants.qml
        #           singleton Values    1.0 Values.qml
        ClusterTutorial/qmldir/Constants.qml
        ClusterTutorial/qmldir/Values.qml
        ClusterTutorial/qmldir/TextItem.qml
        ClusterTutorial/qmldir/TriangleItem.qml
        ClusterTutorial/qmldir/DirectoryFontLoader.qml
        ClusterTutorial/qmldir/Temp_dial_195_83.ui.qml
        ClusterTutorial/qmldir/StarItem.qml
        ClusterTutorial/qmldir/Left_Indicator.ui.qml
        ClusterTutorial/qmldir/StatusLed.qml
        ClusterTutorial/qmldir/HealthBar.qml
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
        # --- 3rdparty qmltype manifests and qmldir module files ---
        3rdparty/Components/plugins.qmltypes
        3rdparty/plugins.qmltypes
        3rdparty/Studio/Components/plugins.qmltypes
        3rdparty/Components/qmldir
        3rdparty/qmldir
        3rdparty/Studio/Components/qmldir

        # --- ClusterTutorial module manifest ---
        # This qmldir must declare Constants and Values as singletons:
        #   singleton Constants 1.0 Constants.qml
        #   singleton Values    1.0 Values.qml
        ClusterTutorial/qmldir/qmldir

        # --- Fonts ---
        ClusterTutorial/qmldir/fonts/IBMPlexMono-ExtraLight.otf
        ClusterTutorial/qmldir/fonts/fonts.txt
        ClusterTutorial/qmldir/fonts/Exo2-Thin.ttf
        ClusterTutorial/qmldir/fonts/Cherry-Monospace-Light.ttf

        # --- Assets ---
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
        ClusterTutorial/qmldir/carLogo.png

        # --- Icons ---
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
        ClusterTutorial/qmldir/icons/glowPlugsOn.png
        ClusterTutorial/qmldir/icons/glowPlugsOff.png
        ClusterTutorial/qmldir/icons/glowEcuOn.png
        ClusterTutorial/qmldir/icons/glowEcuOff.png
        ClusterTutorial/qmldir/icons/signalOil.png
        ClusterTutorial/qmldir/icons/signalOilOn.png
        ClusterTutorial/qmldir/icons/carDoorsClosed.png
        ClusterTutorial/qmldir/icons/carDoorsOpen.png
        
        # --- Wayland protocol XML (bundled as resource for reference) ---
        protocol/agl-shell.xml
)

# =============================================================================
# Install
# =============================================================================

include(GNUInstallDirs)
install(TARGETS ClusterDashboardApp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
