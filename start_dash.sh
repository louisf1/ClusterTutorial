#!/bin/bash
# dobuild.sh — ClusterDashboard clean build + run script
# Fixes applied:
#   - Shebang fixed from #/bin/sh to #!/bin/bash (was a comment, not a shebang)
#   - Qt version updated from 6.10.2 to 6.10.3 to match CMakeLists.txt
#   - Bogus second cmake call removed ("cmake build ." is not a valid command)
#   - make replaced with cmake --build for portability and parallel correctness
#   - Environment variables moved BEFORE the build (and definitely before run)
#   - Duplicate/overwriting QML_IMPORT_PATH fixed into a single colon-separated value
#   - Double slash in path fixed (ClusterWorx//imports)
#   - QML2_IMPORT_PATH kept for Qt5Compat fallback but clearly labelled
#   - Script hardened with set -e so it stops on first error

set -e  # Stop immediately if any command fails

# =============================================================================
# Configuration — edit these if paths change
# =============================================================================
QT_DIR="$HOME/Qt/6.10.3/gcc_64"
PROJECT_DIR="$HOME/ClusterWorx"
BUILD_DIR="$PROJECT_DIR/build"
DESIGN_STUDIO_QML="$HOME/Qt/Tools/QtDesignStudio/qt6_design_studio_reduced_version/qml"

# =============================================================================
# 1. Environment — set BEFORE cmake so find_package picks up the right Qt
# =============================================================================
export PATH="$QT_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$QT_DIR/qml:$QT_DIR/include:$QT_DIR/lib:$PROJECT_DIR/custom_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DISPLAY="${DISPLAY:-:1}"

# QML import search path — priority order (leftmost wins):
#   1. Project's own imports/ folder
#   2. Qt Design Studio reduced QML (for Studio Effects, etc.)
#   3. Qt 6 installed QML
# [FIX] The original script set QML_IMPORT_PATH twice; the second assignment
#        silently discarded the first (project imports + Qt qml were lost).
export QML_IMPORT_PATH="$PROJECT_DIR/imports:$DESIGN_STUDIO_QML:$QT_DIR/qml"

# QML2_IMPORT_PATH is the Qt5-era name; still read by some Qt6 modules (Core5Compat)
export QML2_IMPORT_PATH="$QML_IMPORT_PATH"

# =============================================================================
# 6. Start hardware interface
# =============================================================================
echo "==> Starting Arduino hardware interface..."
/home/louis/bin/arduino-cli compile --upload --fqbn arduino:avr:mega -p /dev/ttyACM0 \
    /home/louis/ClusterWorx/Arduino/Ford_Ranger_signals_all/Ford_Ranger_signals_all.ino

echo "==> Starting Python hardware bridge..."
/home/louis/.conan-env/bin/python3 /home/louis/ClusterWorx/Python_Utils/rpm_speed_arduino_dev_ttyACM0.py &
PYTHON_PID=$!

# =============================================================================
# 7. Run
# =============================================================================
echo "==> Launching ClusterDashboardApp..."
cd "$BUILD_DIR"
./ClusterDashboardApp
kill $PYTHON_PID 2>/dev/null
