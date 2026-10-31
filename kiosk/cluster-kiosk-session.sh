#!/bin/sh
# cluster-kiosk-session.sh — LightDM session entry for the dashboard kiosk.
#
# Same start-up as /usr/bin/labwc-pi (the normal Raspberry Pi desktop), but
# labwc is pointed at kiosk/labwc/ only (-C, no -m merge), so the desktop's
# /etc/xdg/labwc/autostart (pcmanfm-pi desktop, wf-panel-pi taskbar) never
# runs. The only client is the dashboard, started from kiosk/labwc/autostart.
# Text consoles stay available on Ctrl+Alt+F2..F6.

. /usr/bin/setup_env

if raspi-config nonint is_pi ; then
    export WLR_DRM_FORCE_LIBLIFTOFF=1

    if ! raspi-config nonint gpu_has_mmu ; then
        export WLR_RENDERER=pixman
    fi
fi

exec /usr/bin/labwc -C "$(dirname "$(readlink -f "$0")")/labwc"
