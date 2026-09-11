#!/bin/sh
# dwl owns this startup process group and terminates it on session exit.
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
GDK_BACKEND=wayland ./shell/xpwm-desktop &
GDK_BACKEND=wayland ./shell/xpwm-taskbar &
foot --app-id=xpwm-demo --title='XPWm desktop test' &
wait
