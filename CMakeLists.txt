cmake_minimum_required(VERSION 3.21.1)
project(ClusterDashboard VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# Bypass license check for the build process
set(ENV{QTFRAMEWORK_BYPASS_LICENSE_CHECK} 1)

# 2. Handle Custom Libraries (Bypassing find_package for Studio Components)
# Since you have the .so files in custom_libs, we link them directly.
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/custom_libs)

# 2. Find standard Qt modules (ONLY what is in your /home/louis/Qt folder)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickTimeline)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(kuksaclient REQUIRED)


# 3. ALIGN OUTPUT DIRECTORY (Fixes the "should end in target path" warning)
# Your URI is "ClusterTutorial", so the output must end in that path.
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/ClusterTutorial")

# 1. First, define the target as an executable
qt_add_executable(ClusterDashboardApp
    main.cpp
    3rdparty/Studio/Application/quickstudioapplication.cpp
    3rdparty/Components/qtstudiocomponentsplugin.cpp
)

# 1. SILENCE POLICIES (Fixes QTP0001 and QTP0004 warnings)
qt_policy(SET QTP0001 NEW) # Use the new resource prefix logic
qt_policy(SET QTP0004 NEW) # Require qmldir for subdirectories


# 4. Define the QML Module
# The URI "ClusterTutorial" allows you to use "import ClusterTutorial" in QML
qt6_add_qml_module(ClusterDashboardApp
    URI "ClusterTutorial"
    VERSION 1.0
    NO_PLUGIN
    
    # C++ SOURCES: Moved here from qt_add_executable
    SOURCES
        3rdparty/Studio/Application/quickstudioapplication.cpp
        3rdparty/Studio/Application/quickstudioapplication_p.h
        3rdparty/Components/qtstudiocomponentsplugin.cpp
        3rdparty/Components/QtQml/qqmlextensionplugin.h
        
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

# --- BUNDLING STUDIO MODULES AS RESOURCES ---
        # This fixes the "module not installed" errors on AGL
        imports/Qt5Compat/GraphicalEffects/FastBlur.qml
        imports/Qt5Compat/GraphicalEffects/HueSaturation.qml
        imports/Qt5Compat/GraphicalEffects/DirectionalBlur.qml
        imports/Qt5Compat/GraphicalEffects/Displace.qml
        imports/Qt5Compat/GraphicalEffects/private/FastGlow.qml
        imports/Qt5Compat/GraphicalEffects/private/GaussianGlow.qml
        imports/Qt5Compat/GraphicalEffects/private/GaussianDirectionalBlur.qml
        imports/Qt5Compat/GraphicalEffects/private/FastInnerShadow.qml
        imports/Qt5Compat/GraphicalEffects/private/GaussianMaskedBlur.qml
        imports/Qt5Compat/GraphicalEffects/private/GaussianInnerShadow.qml
        imports/Qt5Compat/GraphicalEffects/private/DropShadowBase.qml
        imports/Qt5Compat/GraphicalEffects/DropShadow.qml
        imports/Qt5Compat/GraphicalEffects/GaussianBlur.qml
        imports/Qt5Compat/GraphicalEffects/BrightnessContrast.qml
        imports/Qt5Compat/GraphicalEffects/ThresholdMask.qml
        imports/Qt5Compat/GraphicalEffects/RadialBlur.qml
        imports/Qt5Compat/GraphicalEffects/Colorize.qml
        imports/Qt5Compat/GraphicalEffects/ConicalGradient.qml
        imports/Qt5Compat/GraphicalEffects/RecursiveBlur.qml
        imports/Qt5Compat/GraphicalEffects/MaskedBlur.qml
        imports/Qt5Compat/GraphicalEffects/InnerShadow.qml
        imports/Qt5Compat/GraphicalEffects/RadialGradient.qml
        imports/Qt5Compat/GraphicalEffects/LinearGradient.qml
        imports/Qt5Compat/GraphicalEffects/OpacityMask.qml
        imports/Qt5Compat/GraphicalEffects/RectangularGlow.qml
        imports/Qt5Compat/GraphicalEffects/Blend.qml
        imports/Qt5Compat/GraphicalEffects/Glow.qml
        imports/Qt5Compat/GraphicalEffects/GammaAdjust.qml
        imports/Qt5Compat/GraphicalEffects/ZoomBlur.qml
        imports/Qt5Compat/GraphicalEffects/ColorOverlay.qml
        imports/Qt5Compat/GraphicalEffects/Desaturate.qml
        imports/Qt5Compat/GraphicalEffects/LevelAdjust.qml
        imports/QtQuick/Studio/Effects/MaskedBlurItem.qml
        imports/QtQuick/Studio/Effects/designer/GammaAdjustEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/ZoomBlurEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/LevelAdjustEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/HueSaturationEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/DirectionalBlurEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/DropShadowEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/InnerShadowEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/BrightnessContrastEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/RadialBlurEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/ColorizeEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/BlendEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/OpacityMaskEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/RecursiveBlurEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/ColorOverlayEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/GaussianBlurEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/GlowEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/FastBlurEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/DesaturationEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/MaskedBlurEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/DisplaceEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/designer/ThresholdMaskEffectSpecifics.qml
        imports/QtQuick/Studio/Effects/RadialBlurItem.qml
        imports/QtQuick/Studio/Effects/ColorOverlayItem.qml
        imports/QtQuick/Studio/Effects/GaussianBlurEffect.qml
        imports/QtQuick/Studio/Effects/HueSaturationEffect.qml
        imports/QtQuick/Studio/Effects/DirectionalBlurItem.qml
        imports/QtQuick/Studio/Effects/ZoomBlurEffect.qml
        imports/QtQuick/Studio/Effects/FastBlurItem.qml
        imports/QtQuick/Studio/Effects/OpacityMaskItem.qml
        imports/QtQuick/Studio/Effects/BrightnessContrastItem.qml
        imports/QtQuick/Studio/Effects/ZoomBlurItem.qml
        imports/QtQuick/Studio/Effects/GaussianBlurItem.qml
        imports/QtQuick/Studio/Effects/GlowEffect.qml
        imports/QtQuick/Studio/Effects/DisplaceEffect.qml
        imports/QtQuick/Studio/Effects/ColorizeItem.qml
        imports/QtQuick/Studio/Effects/MaskedBlurEffect.qml
        imports/QtQuick/Studio/Effects/InnerShadowEffect.qml
        imports/QtQuick/Studio/Effects/SaturationItem.qml
        imports/QtQuick/Studio/Effects/GlowItem.qml
##        imports/QtQuick/Studio/Effects/QuickStudioEffects.qmltypes
        imports/QtQuick/Studio/Effects/GammaAdjustEffect.qml
        imports/QtQuick/Studio/Effects/DesaturationEffect.qml
        imports/QtQuick/Studio/Effects/RecursiveBlurItem.qml
        imports/QtQuick/Studio/Effects/BlendItem.qml
        imports/QtQuick/Studio/Effects/ColorizeEffect.qml
        imports/QtQuick/Studio/Effects/DesaturationItem.qml
        imports/QtQuick/Studio/Effects/DropShadowEffect.qml
        imports/QtQuick/Studio/Effects/RecursiveBlurEffect.qml
        imports/QtQuick/Studio/Effects/BrightnessContrastEffect.qml
        imports/QtQuick/Studio/Effects/DisplaceItem.qml
        imports/QtQuick/Studio/Effects/MaskItem.qml
        imports/QtQuick/Studio/Effects/ColorOverlayEffect.qml
        imports/QtQuick/Studio/Effects/LevelAdjustEffect.qml
        imports/QtQuick/Studio/Effects/InnerShadowItem.qml
        imports/QtQuick/Studio/Effects/GammaAdjustItem.qml
        imports/QtQuick/Studio/Effects/HueSaturationItem.qml
        imports/QtQuick/Studio/Effects/RadialBlurEffect.qml
        imports/QtQuick/Studio/Effects/LevelAdjustItem.qml
        imports/QtQuick/Studio/Effects/ThresholdMaskItem.qml
        imports/QtQuick/Studio/Effects/DirectionalBlurEffect.qml
        imports/QtQuick/Studio/Effects/RotationItem.qml
        imports/QtQuick/Studio/Effects/BlendEffect.qml
        imports/QtQuick/Studio/Effects/ThresholdMaskEffect.qml
        imports/QtQuick/Studio/Effects/FastBlurEffect.qml
        imports/QtQuick/Studio/Effects/DropShadowItem.qml
        imports/QtQuick/Studio/Effects/OpacityMaskEffect.qml
        imports/QtQuick/Studio/Components/designer/CapComboBox.qml
        imports/QtQuick/Studio/Components/designer/StrokeDetailsSection.qml
        imports/QtQuick/Studio/Components/designer/DashPatternEditor.qml
        imports/QtQuick/Studio/Components/designer/TriangleItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/SvgPathItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/TextItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/RectangleItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/StraightArrowSpecifics.qml
        imports/QtQuick/Studio/Components/designer/BevelSection.qml
        imports/QtQuick/Studio/Components/designer/ArcArrowSpecifics.qml
        imports/QtQuick/Studio/Components/designer/GroupItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/CornerRadiusSection.qml
        imports/QtQuick/Studio/Components/designer/StarItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/FlipableItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/RegularPolygonItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/ArcItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/BorderItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/EllipseItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/PieItemSpecifics.qml
        imports/QtQuick/Studio/Components/designer/BorderModeComboBox.qml
        imports/QtQuick/Studio/Components/EllipseItem.qml
        imports/QtQuick/Studio/Components/FlipableItem.qml
        imports/QtQuick/Studio/Components/ArcItem.qml
        imports/QtQuick/Studio/Components/RegularPolygonItem.qml
        imports/QtQuick/Studio/Components/StraightArrow.qml
        imports/QtQuick/Studio/Components/TextItem.qml
        imports/QtQuick/Studio/Components/TriangleItem.qml
        imports/QtQuick/Studio/Components/StarItem.qml
        imports/QtQuick/Studio/Components/SvgPathItem.qml
        imports/QtQuick/Studio/Components/GroupItem.qml
        imports/QtQuick/Studio/Components/BorderItem.qml
        imports/QtQuick/Studio/Components/RectangleItem.qml
        imports/QtQuick/Studio/Components/PieItem.qml
        imports/QtQuick/Studio/Components/ArcArrow.qml
        imports/QtQuick/Studio/LogicHelper/designer/OrOperatorSpecifics.qml
        imports/QtQuick/Studio/LogicHelper/designer/StringMapperSpecifics.qml
        imports/QtQuick/Studio/LogicHelper/designer/MinMaxMapperSpecifics.qml
        imports/QtQuick/Studio/LogicHelper/designer/BidirectionalBindingSpecifics.qml
        imports/QtQuick/Studio/LogicHelper/designer/RangeMapperSpecifics.qml
        imports/QtQuick/Studio/LogicHelper/designer/NotOperatorSpecifics.qml
        imports/QtQuick/Studio/LogicHelper/designer/AndOperatorSpecifics.qml
        imports/QtQuick/Studio/LogicHelper/OrOperator.qml
        imports/QtQuick/Studio/LogicHelper/RangeMapper.qml
        imports/QtQuick/Studio/LogicHelper/StringMapper.qml
        imports/QtQuick/Studio/LogicHelper/NotOperator.qml
        imports/QtQuick/Studio/LogicHelper/BidirectionalBinding.qml
##        imports/QtQuick/Studio/LogicHelper/QuickStudioLogicHelper.qmltypes
        imports/QtQuick/Studio/LogicHelper/AndOperator.qml
        imports/QtQuick/Studio/LogicHelper/MinMaxMapper.qml
##        imports/QtQuick/Studio/Timeline/BlendTrees/plugins.qmltypes

##        imports/QtQuick/Studio/Timeline/plugins.qmltypes
##        imports/QtQuick/Studio/Application/plugins.qmltypes
        imports/QtQuick/Studio/Utils/ChildListModel.qml
        imports/QtQuick/Studio/Utils/JsonListModel.qml
##        imports/QtQuick/Studio/Utils/plugins.qmltypes
        imports/QtQuick/Studio/Utils/JsonBackend.qml
        imports/QtQuick/Studio/EventSystem/EventSystem.qml
        imports/QtQuick/Studio/EventSystem/EventListener.qml
##        imports/QtQuick/Studio/EventSystem/QuickStudioEventSystem.qmltypes

        imports/QtQuick/Studio/DesignEffects/designer/DesignEffectPane.qml
        imports/QtQuick/Studio/DesignEffects/designer/DesignDropShadowSpecifics.qml
        imports/QtQuick/Studio/DesignEffects/designer/DesignInnerShadowSpecifics.qml
        imports/QtQuick/Studio/DesignEffects/designer/DesignEffectSpecifics.qml
        imports/QtQuick/Studio/DesignEffects/DesignLayerBlurPrivate.qml
        imports/QtQuick/Studio/DesignEffects/DesignDropShadowPrivate.qml
        imports/QtQuick/Studio/DesignEffects/DesignBackgroundBlurPrivate.qml
        imports/QtQuick/Studio/DesignEffects/DesignDropShadow.qml
        imports/QtQuick/Studio/DesignEffects/DesignInnerShadowPrivate.qml
##        imports/QtQuick/Studio/DesignEffects/QuickStudioDesignEffects.qmltypes
        imports/QtQuick/Studio/DesignEffects/DesignEffect.qml
        imports/QtQuick/Studio/DesignEffects/DesignInnerShadow.qml
        imports/QtQuick/Studio/DesignEffects/DesignEffectPrivate.qml
        imports/QtQuick/Studio/MultiText/designer/MultiTextElementSpecifics.qml
        imports/QtQuick/Studio/MultiText/designer/MultiTextItemSpecifics.qml
        imports/QtQuick/Studio/MultiText/designer/MultiTextExceptionSpecifics.qml
        imports/QtQuick/Studio/MultiText/MultiTextItem.qml
        imports/QtQuick/Studio/MultiText/MultiTextElement.qml
        imports/QtQuick/Studio/MultiText/MultiTextException.qml
##        imports/QtQuick/Studio/MultiText/QuickStudioMultiText.qmltypes
##        imports/QtQuick/Studio/QtQml/XmlListModel/plugins.qmltypes
##        imports/QtQuick/Studio/QtQml/Models/plugins.qmltypes
##        imports/QtQuick/Studio/QtQml/plugins.qmltypes
##        imports/QtQuick/Studio/QtQml/WorkerScript/plugins.qmltypes
        imports/QtQuick/Studio/EventSimulator/EventSimulatorDelegate.qml
        imports/QtQuick/Studio/EventSimulator/EventSimulator.qml
##        imports/QtQuick/Studio/EventSimulator/QuickStudioEventSimulator.qmltypes
        
        3rdparty/ArcItem.qml
##3rdparty//CMakeLists.txt

##3rdparty/qtstudiocomponentsplugin.cpp
        3rdparty/RectangleItem.qml
        3rdparty/StraightArrow.qml
        3rdparty/SvgPathItem.qml
        3rdparty/TriangleItem.qml
        3rdparty/Components/ArcItem.qml
##3rdparty/Components/CMakeLists.txt

##3rdparty/Components/qtstudiocomponentsplugin.cpp
        3rdparty/Components/RectangleItem.qml
        3rdparty/Components/StraightArrow.qml
        3rdparty/Components/SvgPathItem.qml
        3rdparty/Components/TriangleItem.qml

##3rdparty/Studio/Application/
##3rdparty/Studio/Application/CMakeLists.txt
##3rdparty/Studio/Application/quickstudioapplication.cpp
##3rdparty/Studio/Application/quickstudioapplication_p.h

        3rdparty/Studio/Components/ArcItem.qml
##3rdparty/Studio/Components/CMakeLists.txt
##3rdparty/Studio/Components/qtstudiocomponentsplugin.cpp
        3rdparty/Studio/Components/RectangleItem.qml
        3rdparty/Studio/Components/StraightArrow.qml
        3rdparty/Studio/Components/SvgPathItem.qml
        3rdparty/Studio/Components/TriangleItem.qml


    RESOURCES
        3rdparty/plugins.qmltypes
        3rdparty/Components/plugins.qmltypes
        3rdparty/Studio/Components/plugins.qmltypes

        imports/Qt5Compat/GraphicalEffects/private/plugins.qmltypes
        imports/QtQuick/Studio/MultiText/QuickStudioMultiText.qmltypes
        imports/QtQuick/Studio/QtQml/XmlListModel/plugins.qmltypes
        imports/QtQuick/Studio/QtQml/Models/plugins.qmltypes
        imports/QtQuick/Studio/QtQml/plugins.qmltypes
        imports/QtQuick/Studio/QtQml/WorkerScript/plugins.qmltypes
        imports/QtQuick/Studio/EventSimulator/QuickStudioEventSimulator.qmltypes
        imports/QtQuick/Studio/Timeline/BlendTrees/plugins.qmltypes
        imports/QtQuick/Studio/Timeline/plugins.qmltypes
        imports/QtQuick/Studio/Application/plugins.qmltypes
        imports/QtQuick/Studio/Utils/plugins.qmltypes
        imports/QtQuick/Studio/EventSystem/QuickStudioEventSystem.qmltypes
        imports/QtQuick/Studio/DesignEffects/QuickStudioDesignEffects.qmltypes
        imports/QtQuick/Studio/Components/QuickStudioComponents.qmltypes
        imports/Qt5Compat/GraphicalEffects/plugins.qmltypes


       ## the qmldir modules link file
        ClusterTutorial/qmldir/qmldir
        3rdparty/Studio/Components/qmldir
        3rdparty/Components/qmldir        

        imports/Qt5Compat/GraphicalEffects/private/qmldir
        imports/Qt5Compat/GraphicalEffects/qmldir
        imports/QtQuick/Studio/DesignEffects/qmldir
        imports/QtQuick/Studio/QtQml/Models/qmldir
        imports/QtQuick/Studio/MultiText/qmldir
        imports/QtQuick/Studio/QtQml/XmlListModel/qmldir
        imports/QtQuick/Studio/QtQml/WorkerScript/qmldir
        imports/QtQuick/Studio/QtQml/qmldir        
        imports/QtQuick/Studio/EventSimulator/qmldir
        imports/QtQuick/Studio/EventSystem/qmldir
        imports/QtQuick/Studio/LogicHelper/qmldir
        imports/QtQuick/Studio/Timeline/BlendTrees/qmldir
        imports/QtQuick/Studio/Timeline/qmldir
        imports/QtQuick/Studio/Application/qmldir
        imports/QtQuick/Studio/Utils/qmldir        
        imports/QtQuick/Studio/Components/qmldir                                        
        imports/QtQuick/Studio/Effects/qmldir
                                        
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

# 4. Include Headers for 3rdparty components
target_include_directories(ClusterDashboardApp PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Studio/Application
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/Components
)

# 5. Link everything to the Executable
target_link_libraries(ClusterDashboardApp PRIVATE 
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickTimeline
    # Manually linking your custom .so files by name
    Qt6QuickStudioComponents
    Qt6QuickStudioEffects
    gRPC::grpc++
    protobuf::libprotobuf
    kuksaclient
)

# 6. Install logic
include(GNUInstallDirs)
install(TARGETS ClusterDashboardApp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
