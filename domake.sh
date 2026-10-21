#!/bin/bash
# domake.sh — ClusterDashboard incremental build + run script
# Kept in sync with dobuild.sh (2026-08-06); the only difference is that
# domake.sh never cleans — use dobuild.sh --clean for a from-scratch build.
# Fixes carried over from dobuild.sh:
#   - Qt comes from Debian bookworm arm64 packages (qt6-base-dev 6.4.2), not
#     the x86-64 desktop Qt in ~/Qt (cannot execute on this aarch64 board)
#   - Hardcoded /home/louis/... paths replaced with $HOME/$PROJECT_DIR
#   - Build parallelism capped at 2: 2 GiB RAM board, 6 jobs trip the OOM killer
#   - QT_QUICK_BACKEND=software: Debian Qt is desktop-GL, Mali blob is ES-only
#   - VSS_paths.txt staged copy-if-missing (the app persists trip data into it)
#   - Arduino port auto-detected on ttyACM0/ttyACM1, upload skipped if absent
#   - PATH prepends ~/.local/bin for the pip-installed CMake >= 3.22

set -e
trap 'kill $(jobs -p) 2>/dev/null; pkill -f avrdude 2>/dev/null; pkill -f arduino-cli 2>/dev/null' EXIT

# =============================================================================
# Configuration — edit these if paths change
# =============================================================================
QT_SYSTEM_BIN="/usr/lib/qt6/bin"
QT_SYSTEM_QML="/usr/lib/aarch64-linux-gnu/qt6/qml"
PROJECT_DIR="$HOME/ClusterWorx"
BUILD_DIR="$PROJECT_DIR/build"

# =============================================================================
# 1. Environment — set BEFORE cmake so find_package picks up the right Qt
# =============================================================================
export PATH="$HOME/.local/bin:$QT_SYSTEM_BIN:$PATH"
export LD_LIBRARY_PATH="$PROJECT_DIR/custom_libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

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
# 2. Build directory — never cleaned here (use dobuild.sh --clean for that)
# =============================================================================
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# =============================================================================
# 3. Configure
# =============================================================================
echo "==> Configuring..."
cmake ..

# =============================================================================
# 4. Build
# =============================================================================
# [FIX] Parallelism capped at 2 (was $(nproc)=6): the board has ~2 GiB RAM and
#        6 concurrent cc1plus jobs on the generated qmlcache C++ files trip
#        the kernel OOM killer ("c++: fatal error: Killed signal").
echo "==> Building..."
cmake --build . --parallel 2

# =============================================================================
# 5. Stage runtime assets
# =============================================================================
# [FIX] Copy only if missing: the app persists runtime state (trip meter,
#        odometer, VIN) back into the build-dir copy, which must survive
#        rebuilds.
echo "==> Staging assets..."
[ -f VSS_paths.txt ] || cp "$PROJECT_DIR/VSS_paths.txt" .

# =============================================================================
# 6. KUKSA databroker — normally already up at boot (docker.service is enabled
#    and the container has restart: unless-stopped). Start whichever is down.
# =============================================================================
echo "==> Checking KUKSA databroker..."
if ! systemctl is-active --quiet docker; then
    echo "==> Docker service is down — starting it"
    sudo systemctl start docker || echo "⚠️  Could not start docker.service"
fi
# Plain docker needs the docker group in this session; fall back to sudo.
DOCKER="docker"
$DOCKER info >/dev/null 2>&1 || DOCKER="sudo docker"
if [ "$($DOCKER inspect -f '{{.State.Running}}' kuksa-databroker 2>/dev/null)" != "true" ]; then
    echo "==> kuksa-databroker is not running — starting it"
    (cd "$PROJECT_DIR" && $DOCKER compose up -d databroker) || \
        echo "⚠️  Could not start kuksa-databroker — dashboard will run without KUKSA"
else
    echo "==> kuksa-databroker is running"
fi

# =============================================================================
# 7. Start hardware interface
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
# 8. Run
# =============================================================================
echo "==> Launching ClusterDashboardApp..."
./ClusterDashboardApp
kill $PYTHON_PID 2>/dev/null
