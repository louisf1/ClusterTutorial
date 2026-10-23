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
# (2341:8036) is also CDC-ACM and often grabs ttyACM0. Matches the Mega's
# product IDs (2341:0042 Mega 2560 R3, 2341:0010 Mega 2560) — same set as
# MEGA_PIDS in rpm_speed_arduino_dev_ttyACM0.py.
PORT=""
for candidate in /dev/ttyACM*; do
    [ -e "$candidate" ] || continue
    props=$(udevadm info -q property -n "$candidate" 2>/dev/null)
    vid=$(sed -n 's/^ID_VENDOR_ID=//p' <<< "$props")
    pid=$(sed -n 's/^ID_MODEL_ID=//p' <<< "$props")
    if [ "$vid" = "2341" ] && { [ "$pid" = "0042" ] || [ "$pid" = "0010" ]; }; then
        PORT="$candidate"
        break
    fi
done
if [ -z "$PORT" ]; then
    echo "==> No Arduino Mega (USB 2341:0042/0010) found — is the board plugged in?" >&2
    exit 1
fi
echo "==> Found Arduino Mega on $PORT"

WAS_ACTIVE=false
if systemctl is-active --quiet "$BRIDGE"; then
    WAS_ACTIVE=true
fi

# start_dash.sh runs the bridge as its own background process, not via
# systemd. Pause it (SIGSTOP) rather than kill it: a stopped process doesn't
# read the port, so avrdude gets the bootloader bytes, and start_dash.sh's
# `kill $PYTHON_PID` still works on dashboard exit. The Mega's USB link stays
# up across the reset, so the bridge keeps its open port after SIGCONT.
PAUSED_PIDS=""

restore_bridge() {
    if $WAS_ACTIVE; then
        echo "==> Restarting $BRIDGE..."
        sudo systemctl start "$BRIDGE"
    fi
    if [ -n "$PAUSED_PIDS" ]; then
        echo "==> Resuming start_dash bridge (PID $PAUSED_PIDS)..."
        kill -CONT $PAUSED_PIDS 2>/dev/null || true
    fi
}
trap restore_bridge EXIT

if $WAS_ACTIVE; then
    echo "==> Stopping $BRIDGE to free $PORT..."
    sudo systemctl stop "$BRIDGE"
    sleep 1
fi

PAUSED_PIDS=$(pgrep -f "Python_Utils/rpm_speed_arduino_dev_ttyACM0.py" | tr '\n' ' ' | sed 's/ $//' || true)
if [ -n "$PAUSED_PIDS" ]; then
    echo "==> Pausing start_dash bridge (PID $PAUSED_PIDS) to free $PORT..."
    kill -STOP $PAUSED_PIDS
    sleep 1
fi

echo "==> Compiling and uploading $SKETCH..."
/home/louis/bin/arduino-cli compile --upload --fqbn "$FQBN" -p "$PORT" "$SKETCH"

echo "==> Done. Board will re-enumerate on $PORT in a couple seconds."
