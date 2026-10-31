#!/bin/bash
# run_dashboard.sh — keep the dashboard running inside the kiosk session.
# Runs start_dash.sh (KUKSA check, Mega upload, Python bridge, dashboard) and
# restarts it if the dashboard exits or crashes. Output goes to
# /tmp/cluster-dash.log (view from a text console: tail -f /tmp/cluster-dash.log).

LOG=/tmp/cluster-dash.log

while true; do
    echo "==> $(date '+%F %T') starting start_dash.sh" >> "$LOG"
    "$HOME/ClusterWorx/start_dash.sh" >> "$LOG" 2>&1
    echo "==> $(date '+%F %T') start_dash.sh exited ($?) — restarting in 5s" >> "$LOG"
    sleep 5
done
