#!/bin/bash
# enable_persistent_journal.sh — keep the system journal across reboots.
#
# Raspberry Pi OS keeps the journal in RAM only
# (/usr/lib/systemd/journald.conf.d/40-rpi-volatile-storage.conf), so every
# reboot wipes the USB, voltage and thermal records needed to diagnose a
# dashboard dropout. This drop-in overrides that: later filenames win.
#
# Usage: ./enable_persistent_journal.sh   (asks for your sudo password)
# Safe to run more than once. To undo: sudo rm "$CONF" and restart
# systemd-journald.

set -e

CONF_DIR="/etc/systemd/journald.conf.d"
CONF="$CONF_DIR/99-persistent.conf"

echo "==> Writing $CONF"
sudo install -d "$CONF_DIR"
printf '[Journal]\nStorage=persistent\nSystemMaxUse=200M\nSyncIntervalSec=30s\n' | sudo tee "$CONF"

echo "==> Restarting systemd-journald"
sudo systemctl restart systemd-journald
sudo journalctl --flush

echo "==> Journal files now on disk:"
sudo ls -la /var/log/journal/*/
journalctl --disk-usage
