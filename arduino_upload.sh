#!/bin/bash
# arduino_upload.sh — compile & upload an Arduino sketch to /dev/ttyACM0
#
# arduino-bridge.service holds /dev/ttyACM0 open (confirmed via lsof), so
# arduino-cli/Arduino IDE can't claim the port to reset+flash the board while
# it's running. This script stops the bridge for the upload and restarts it
# afterwards — on success, failure, or Ctrl-C — via an EXIT trap.
#
# Usage: ./arduino_upload.sh [path/to/sketch.ino]
#   Defaults to Ford_Ranger_signals_all.ino if no sketch is given.

set -e

SKETCH="${1:-$HOME/ClusterWorx/Arduino/Ford_Ranger_signals_all/Ford_Ranger_signals_all.ino}"
PORT="/dev/ttyACM1"
FQBN="arduino:avr:mega"
BRIDGE="arduino-bridge.service"

WAS_ACTIVE=false
if systemctl --user is-active --quiet "$BRIDGE"; then
    WAS_ACTIVE=true
fi

restore_bridge() {
    if $WAS_ACTIVE; then
        echo "==> Restarting $BRIDGE..."
        systemctl --user start "$BRIDGE"
    fi
}
trap restore_bridge EXIT

if $WAS_ACTIVE; then
    echo "==> Stopping $BRIDGE to free $PORT..."
    systemctl --user stop "$BRIDGE"
    sleep 1
fi

echo "==> Compiling and uploading $SKETCH..."
/home/louis/bin/arduino-cli compile --upload --fqbn "$FQBN" -p "$PORT" "$SKETCH"

echo "==> Done. Board will re-enumerate on $PORT in a couple seconds."
