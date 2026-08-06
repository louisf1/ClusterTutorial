#!/bin/bash
# start_dash.sh — launch-only script: no build, just hardware + dashboard.
# Assumes ClusterDashboardApp has already been built (run dobuild.sh or
# domake.sh first).
# Kept in sync with dobuild.sh (2026-08-06):
#   - Qt comes from Debian bookworm arm64 packages, not the x86-64 Qt in ~/Qt
#   - Hardcoded /home/louis/... paths replaced with $HOME/$PROJECT_DIR
#   - QT_QUICK_BACKEND=software: Debian Qt is desktop-GL, Mali blob is ES-only
#   - VSS_paths.txt staged copy-if-missing (the app persists trip data into it)
#   - Arduino port auto-detected on ttyACM0/ttyACM1, upload skipped if absent

set -e
trap 'kill $(jobs -p) 2>/dev/null; pkill -f avrdude 2>/dev/null; pkill -f arduino-cli 2>/dev/null' EXIT

# =============================================================================
# Configuration — edit these if paths change
# =============================================================================
QT_SYSTEM_QML="/usr/lib/aarch64-linux-gnu/qt6/qml"
PROJECT_DIR="$HOME/ClusterWorx"
BUILD_DIR="$PROJECT_DIR/build"

# =============================================================================
# 1. Environment
# =============================================================================
export LD_LIBRARY_PATH="$PROJECT_DIR/custom_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DISPLAY="${DISPLAY:-:0}"

# QML import search path — priority order (leftmost wins):
#   1. Project's own imports/ folder
#   2. System Qt 6 QML modules
export QML_IMPORT_PATH="$PROJECT_DIR/imports:$QT_SYSTEM_QML"

# QML2_IMPORT_PATH is the Qt5-era name; still read by some Qt6 modules (Core5Compat)
export QML2_IMPORT_PATH="$QML_IMPORT_PATH"

# [FIX] Software rendering is required on this board: Debian's arm64 Qt 6.4.2
#       is a desktop-OpenGL build, but the Tinker Board 2 Mali-T860 blob is
#       OpenGL ES-only, so hardware context creation always fails
#       (eglChooseConfig returns 0 configs -> "Failed to create RHI").
#       Remove this if Qt is ever rebuilt with -opengl es2 (GLES support).
export QT_QUICK_BACKEND=software

# =============================================================================
# 2. Sanity check + stage runtime assets
# =============================================================================
if [ ! -x "$BUILD_DIR/ClusterDashboardApp" ]; then
    echo "❌ $BUILD_DIR/ClusterDashboardApp not found — run dobuild.sh first" >&2
    exit 1
fi
cd "$BUILD_DIR"

# [FIX] Copy only if missing: the app persists runtime state (trip meter,
#        odometer, VIN) back into the build-dir copy, which must survive
#        rebuilds.
[ -f VSS_paths.txt ] || cp "$PROJECT_DIR/VSS_paths.txt" .

# =============================================================================
# 3. Start hardware interface
# =============================================================================
echo "==> Starting Arduino hardware interface..."
set +e
ARDUINO_PORT=""
for _p in /dev/ttyACM0 /dev/ttyACM1; do
    if [ -e "$_p" ]; then ARDUINO_PORT="$_p"; break; fi
done
if [ -z "$ARDUINO_PORT" ]; then
    echo "⚠️  No Arduino found on ttyACM0 or ttyACM1 — skipping upload"
else
    echo "==> Uploading to Arduino on $ARDUINO_PORT"
    "$HOME/bin/arduino-cli" compile --upload --fqbn arduino:avr:mega -p "$ARDUINO_PORT" \
        "$PROJECT_DIR/Arduino/Ford_Ranger_signals_all/Ford_Ranger_signals_all.ino"
fi
set -e
sleep 2  # wait for Arduino to reset and re-enumerate

echo "==> Starting Python hardware bridge..."
"$HOME/.conan-env/bin/python3" -u "$PROJECT_DIR/Python_Utils/rpm_speed_arduino_dev_ttyACM0.py" &
PYTHON_PID=$!

# =============================================================================
# 4. Run
# =============================================================================
echo "==> Launching ClusterDashboardApp..."
./ClusterDashboardApp
kill $PYTHON_PID 2>/dev/null
