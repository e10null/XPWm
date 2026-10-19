#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
export XPWM_SOUND_MUTE=1
if [ "${XPWM_MENU_STRUCTURE_TEST:-}" = 1 ]; then
    GDK_BACKEND=wayland ./shell/start-menu-test --menu || exit 1
fi
GDK_BACKEND=wayland ./shell/xpwm-desktop &
GDK_BACKEND=wayland ./shell/xpwm-taskbar &
exec foot --app-id=xpwm-test --title='XPWm decoration test' sh -c 'printf "Native Wayland client\nLuna decoration prototype\n"; sleep 30'
