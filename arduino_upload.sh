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

# Board enumerates as ttyACM0 or ttyACM1 depending on USB state — same two
# ports the Python bridge tries (see SERIAL_PORTS in
# rpm_speed_arduino_dev_ttyACM0.py). Pick whichever exists.
PORT=""
for candidate in /dev/ttyACM0 /dev/ttyACM1; do
    if [ -e "$candidate" ]; then
        PORT="$candidate"
        break
    fi
done
if [ -z "$PORT" ]; then
    echo "==> No /dev/ttyACM0 or /dev/ttyACM1 found — is the board plugged in?" >&2
    exit 1
fi

WAS_ACTIVE=false
if sudo systemctl is-active --quiet "$BRIDGE"; then
    WAS_ACTIVE=true
fi

restore_bridge() {
    if $WAS_ACTIVE; then
        echo "==> Restarting $BRIDGE..."
        sudo systemctl start "$BRIDGE"
    fi
}
trap restore_bridge EXIT

if $WAS_ACTIVE; then
    echo "==> Stopping $BRIDGE to free $PORT..."
    sudo systemctl stop "$BRIDGE"
    sleep 1
fi

echo "==> Compiling and uploading $SKETCH..."
/home/louis/bin/arduino-cli compile --upload --fqbn "$FQBN" -p "$PORT" "$SKETCH"

echo "==> Done. Board will re-enumerate on $PORT in a couple seconds."
