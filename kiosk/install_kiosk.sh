#!/bin/bash
# install_kiosk.sh — boot straight into the dashboard instead of the desktop.
#
#   sudo ./kiosk/install_kiosk.sh             install + make kiosk the autologin session
#   sudo ./kiosk/install_kiosk.sh --desktop   switch autologin back to the normal desktop
#
# Registers kiosk/cluster-kiosk-session.sh as a Wayland session and points
# LightDM's autologin at it. The normal desktop session (rpd-labwc) stays
# installed, so switching back is just --desktop. Reboot to apply.

set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "Run with sudo: sudo $0 $*" >&2
    exit 1
fi

KIOSK_DIR="$(dirname "$(readlink -f "$0")")"
SESSION_FILE=/usr/share/wayland-sessions/cluster-kiosk.desktop
LIGHTDM_CONF=/etc/lightdm/lightdm.conf

set_autologin_session() {
    sed -i "s/^autologin-session=.*/autologin-session=$1/; s/^user-session=.*/user-session=$1/" "$LIGHTDM_CONF"
    grep -E "^(autologin-session|user-session)=" "$LIGHTDM_CONF"
}

if [ "$1" = "--desktop" ]; then
    echo "==> Autologin back to the Raspberry Pi desktop"
    set_autologin_session rpd-labwc
    echo "==> Done — reboot to apply"
    exit 0
fi

chmod +x "$KIOSK_DIR/cluster-kiosk-session.sh" "$KIOSK_DIR/run_dashboard.sh"

echo "==> Installing $SESSION_FILE"
cat > "$SESSION_FILE" <<EOF
[Desktop Entry]
Name=ClusterWorx Dashboard Kiosk
Comment=Dashboard only, no desktop
Exec=$KIOSK_DIR/cluster-kiosk-session.sh
Type=Application
DesktopNames=labwc;wlroots
EOF

[ -f "$LIGHTDM_CONF.pre-kiosk" ] || cp "$LIGHTDM_CONF" "$LIGHTDM_CONF.pre-kiosk"
echo "==> Autologin to the kiosk session (backup: $LIGHTDM_CONF.pre-kiosk)"
set_autologin_session cluster-kiosk
echo "==> Done — reboot to boot straight into the dashboard"
