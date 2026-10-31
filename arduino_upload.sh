#!/bin/bash
# arduino_upload.sh — compile & upload an Arduino sketch to the Mega
#
# arduino-bridge.service holds the board's serial port open (confirmed via
# lsof), so
# arduino-cli/Arduino IDE can't claim the port to reset+flash the board while
# it's running. This script stops the bridge for the upload and restarts it
# afterwards — on success, failure, or Ctrl-C — via an EXIT trap.
#
# Usage: ./arduino_upload.sh [path/to/sketch.ino]
#   Defaults to Ford_Ranger_signals_all.ino if no sketch is given.

set -e

SKETCH="${1:-$HOME/ClusterWorx/Arduino/Ford_Ranger_signals_all/Ford_Ranger_signals_all.ino}"
FQBN="arduino:avr:mega"
BRIDGE="arduino-bridge.service"

# Pick the Mega by USB ID, not by ttyACM number: the PC_Wake_Leonardo
# (2341:8036) is also CDC-ACM and often grabs ttyACM0. Matches genuine Megas
# (2341:0042 Mega 2560 R3, 2341:0010 Mega 2560) on ttyACM*, and CH340-based
# Mega clones (1a86:7523), which enumerate as ttyUSB* instead.
PORT=""
for candidate in /dev/ttyACM* /dev/ttyUSB*; do
    [ -e "$candidate" ] || continue
    props=$(udevadm info -q property -n "$candidate" 2>/dev/null)
    vid=$(sed -n 's/^ID_VENDOR_ID=//p' <<< "$props")
    pid=$(sed -n 's/^ID_MODEL_ID=//p' <<< "$props")
    case "$vid:$pid" in
        2341:0042|2341:0010|1a86:7523)
            PORT="$candidate"
            break
            ;;
    esac
done
if [ -z "$PORT" ]; then
    echo "==> No Arduino Mega (USB 2341:0042/0010 or CH340 1a86:7523) found — is the board plugged in?" >&2
    exit 1
fi
echo "==> Found Arduino Mega on $PORT"

WAS_ACTIVE=false
if systemctl is-active --quiet "$BRIDGE"; then
    WAS_ACTIVE=true
fi

# start_dash.sh runs the bridge as its own background process, not via
# systemd. Stop it outright: merely pausing it (SIGSTOP) leaves the port open,
# and some Megas' bootloaders then never answer avrdude's sync. Its cwd and
# log are recorded first so it can be relaunched the same way afterwards;
# start_dash.sh stops it by name on dashboard exit, so the relaunched copy is
# still cleaned up.
BRIDGE_SCRIPT="Python_Utils/rpm_speed_arduino_dev_ttyACM0.py"
BRIDGE_PYTHON="$HOME/.conan-env/bin/python3"
DASH_BRIDGE_STOPPED=false
DASH_BRIDGE_CWD="$HOME/ClusterWorx/build"
DASH_BRIDGE_LOG="/tmp/cluster-dash.log"

restore_bridge() {
    if $WAS_ACTIVE; then
        echo "==> Restarting $BRIDGE..."
        sudo systemctl start "$BRIDGE"
    fi
    if $DASH_BRIDGE_STOPPED; then
        sleep 2  # let the Mega finish resetting before the bridge opens it
        echo "==> Restarting start_dash bridge (log: $DASH_BRIDGE_LOG)..."
        # Redirect the whole subshell, not just python: otherwise the forked
        # subshell keeps this script's stdout open and a caller piping our
        # output (e.g. `| tail`) hangs until the bridge exits.
        (cd "$DASH_BRIDGE_CWD" && ARDUINO_PORT="$PORT" setsid nohup \
            "$BRIDGE_PYTHON" -u "$HOME/ClusterWorx/$BRIDGE_SCRIPT" &) \
            >> "$DASH_BRIDGE_LOG" 2>&1 < /dev/null
    fi
}
trap restore_bridge EXIT

if $WAS_ACTIVE; then
    echo "==> Stopping $BRIDGE to free $PORT..."
    sudo systemctl stop "$BRIDGE"
    sleep 1
fi

DASH_BRIDGE_PIDS=$(pgrep -f "$BRIDGE_SCRIPT" | tr '\n' ' ' | sed 's/ $//' || true)
if [ -n "$DASH_BRIDGE_PIDS" ]; then
    first_pid=${DASH_BRIDGE_PIDS%% *}
    DASH_BRIDGE_CWD=$(readlink "/proc/$first_pid/cwd" 2>/dev/null || echo "$DASH_BRIDGE_CWD")
    log=$(readlink "/proc/$first_pid/fd/1" 2>/dev/null || true)
    [ -f "$log" ] && DASH_BRIDGE_LOG="$log"

    echo "==> Stopping start_dash bridge (PID $DASH_BRIDGE_PIDS) to free $PORT..."
    DASH_BRIDGE_STOPPED=true
    kill $DASH_BRIDGE_PIDS 2>/dev/null || true
    for _ in 1 2 3 4 5; do
        pgrep -f "$BRIDGE_SCRIPT" > /dev/null || break
        sleep 1
    done
    pkill -KILL -f "$BRIDGE_SCRIPT" 2>/dev/null || true
    sleep 1
fi

echo "==> Compiling and uploading $SKETCH..."
/home/louis/bin/arduino-cli compile --upload --fqbn "$FQBN" -p "$PORT" "$SKETCH"

echo "==> Done. Board will re-enumerate on $PORT in a couple seconds."
