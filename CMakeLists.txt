cmake_minimum_required(VERSION 3.21.1)
project(ClusterDashboard VERSION 1.0 LANGUAGES CXX)

# 1. Setup Environment
set(CMAKE_AUTOMOC ON)
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml)

# 2. Find Packages, Find gRPC, Protobuf and Kuksa Client
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

##find_package(KuksaClientCpp REQUIRED) # Ensure the AGL SDK is sourced

find_package(kuksaclient REQUIRED)

# 3. Define the QML Module FIRST
# This creates the "ClusterTutorial" target that the app will link to
# Ensure this matches the install logs

qt6_add_qml_module(ClusterTutorial
    URI "ClusterTutorial"
    VERSION 1.0
    NO_PLUGIN
    QML_FILES
        ClusterTutorial/qmldir/absIcon.ui.qml
        ClusterTutorial/qmldir/fuelIcon.ui.qml
        ClusterTutorial/qmldir/iceIcon.ui.qml
        ClusterTutorial/qmldir/parkingBrakeIcon.ui.qml
        ClusterTutorial/qmldir/parkingLightIcon.ui.qml
        ClusterTutorial/qmldir/seatbelt.ui.qml
        ClusterTutorial/qmldir/Left_Indicator.qml
        ClusterTutorial/qmldir/Right_Indicator.qml
        ClusterTutorial/qmldir/App.qml
        ClusterTutorial/qmldir/Cluster_Art.ui.qml
        ClusterTutorial/qmldir/Cherry.qml
        ClusterTutorial/qmldir/Gearbox_195_196.ui.qml
        ClusterTutorial/qmldir/Fuel_dial_195_43.ui.qml
        ClusterTutorial/qmldir/Iso_195_157.ui.qml
        ClusterTutorial/qmldir/Iso_195_156.ui.qml
        ClusterTutorial/qmldir/IsoIcon.ui.qml
        ClusterTutorial/qmldir/Backgrounds_195_610.ui.qml
        ClusterTutorial/qmldir/Rpm_dial_195_83.ui.qml
        ClusterTutorial/qmldir/Temp_dial_195_83.ui.qml        
        ClusterTutorial/qmldir/Speed_dial_195_151.ui.qml
        ClusterTutorial/qmldir/CustomDot.qml
        ClusterTutorial/qmldir/ClusterTutorial.qml
        ClusterTutorial/qmldir/EventListModel.qml
        ClusterTutorial/qmldir/Constants.qml
        ClusterTutorial/qmldir/DirectoryFontLoader.qml
        ClusterTutorial/qmldir/EventListSimulator.qml
        ClusterTutorial/qmldir/Values.qml
 )

# 4. Create the Executable Target
qt_add_executable(ClusterDashboardApp main.cpp)

# 5. Link everything to the Executable
# We link the Qt libraries AND the QML module we just defined
target_link_libraries(ClusterDashboardApp PRIVATE 
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    ClusterTutorial
    # Add these three for KUKSA support
    gRPC::grpc++
    protobuf::libprotobuf
    kuksaclient  ###::kuksaclient
)

# 6. Install logic for AGL
include(GNUInstallDirs)
install(TARGETS ClusterDashboardApp 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
