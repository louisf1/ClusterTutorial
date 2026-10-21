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
#   - PATH now prepends $HOME/.local/bin so the pip-installed CMake (>=3.22)
#     is used instead of the system CMake 3.18.4, which Qt 6.10.3 refuses to
#     configure with (`CMake 3.22 or higher is required to use Qt`)
#   - Hardcoded /home/louis/... paths (arduino-cli, conan-env python, hardware
#     bridge script) replaced with $HOME/$PROJECT_DIR — this machine's user is
#     "linaro", and no such louis paths exist here

set -e
trap 'kill $(jobs -p) 2>/dev/null; pkill -f avrdude 2>/dev/null; pkill -f arduino-cli 2>/dev/null' EXIT

# =============================================================================
# Configuration — edit these if paths change
# =============================================================================
# [FIX] Qt now comes from Debian bookworm packages (qt6-base-dev 6.4.2 etc.,
#       installed by install_dependencies.sh) in standard system paths.
#       History: ~/Qt/6.10.3/gcc_64 was the x86-64 desktop Qt — this board is
#       aarch64, so those binaries cannot execute here (qmlimportscanner died
#       with 'Syntax error: "(" unexpected' = kernel exec failure on
#       wrong-arch ELF). The official arm64 builds don't work either: Qt >=6.9
#       needs glibc >=2.38 (board maxes out at bookworm's 2.36) and Qt 6.8.3's
#       tools segfault / need GLIBC_2.38 inconsistently. Debian's own arm64
#       Qt 6.4.2 packages are built for exactly this glibc and just work.
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
# [FIX] Removed the Qt Design Studio reduced QML path — that install is
#       x86-64 (came with the desktop Qt) and its plugin .so files can never
#       load on this aarch64 board. The Studio components the app needs are
#       bundled in the project's own QML module (3rdparty/...).
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
# 2. Build directory — incremental by default, wiped only with --clean
# =============================================================================
# [FIX] The unconditional rm -rf made every run a full rebuild (slow on this
#        board) and threw away all progress if a build was interrupted.
#        Pass --clean for the old behaviour when you need a from-scratch build.
if [ "${1:-}" = "--clean" ]; then
    echo "==> Cleaning build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# =============================================================================
# 3. Configure
# =============================================================================
# [FIX] CMAKE_PREFIX_PATH is already set in CMakeLists.txt but passing it here
#        too makes this script self-contained if you ever remove it from the file.
# [FIX] Removed the invalid second cmake call: "cmake build -D... ."
#        "cmake build" is not a cmake subcommand — it was silently doing nothing
#        (or erroring, depending on cmake version).
echo "==> Configuring..."
cmake ..

# =============================================================================
# 4. Build
# =============================================================================
# [FIX] Using cmake --build instead of bare make:
#        - Works regardless of whether the generator is Make, Ninja, etc.
#        - --parallel is the portable equivalent of make -jN
# [FIX] Parallelism capped at 2 (was $(nproc)=6): the board has ~2 GiB RAM and
#        no swap, and 6 concurrent cc1plus jobs on the generated qmlcache C++
#        files trip the kernel OOM killer ("c++: fatal error: Killed signal").
echo "==> Building..."
cmake --build . --parallel 2

# =============================================================================
# 5. Stage runtime assets
# =============================================================================
# [FIX] Re-enabled: main.cpp loads VSS_paths.txt from the app dir; without it
#        KuksaHandler starts with no signal mappings and the dials stay dead.
#        Copy only if missing: the app persists runtime state (trip meter,
#        odometer, VIN) back into the build-dir copy, which must survive
#        rebuilds. (--clean wipes it, so a clean build re-seeds from the repo.)
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
