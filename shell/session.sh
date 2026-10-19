#!/bin/sh
# dwl owns this startup process group and terminates it on session exit.
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
XPWM_PROJECT_ROOT="$project_root"
. "$project_root/tools/session-theme.sh"
# Restore only previously confirmed settings for matching connected displays.
./shell/xpwm-displayctl --restore || true
if [ "${XPWM_SKIP_LOGON:-0}" != 1 ]; then
	# XP's Welcome screen is a shell surface, not an authentication provider.
	# The display manager/TTY has already authenticated this Linux session.
	GDK_BACKEND=wayland ./shell/xpwm-logon
else
	./shell/xpwm-sound startup
fi
GDK_BACKEND=wayland ./shell/xpwm-desktop &
GDK_BACKEND=wayland ./shell/xpwm-taskbar &
if [ "${XPWM_DEMO:-0}" = 1 ]; then
	foot --app-id=xpwm-demo --title='XPWm desktop test' &
fi
wait
