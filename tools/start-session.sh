#!/bin/sh
# Set the session environment before the compositor forks any launchers.
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
export XPWM_PROJECT_ROOT="$project_root"
. "$project_root/tools/session-theme.sh"
mode=${1:-tty}
if [ "$#" -gt 0 ]; then shift; fi
# Keep notification ownership local to this WM, also when nested in another DE.
if [ "${XPWM_SESSION_BUS:-0}" != 1 ]; then
    export XPWM_SESSION_BUS=1
    exec dbus-run-session -- sh "$project_root/tools/start-session.sh" "$mode" "$@"
fi
case "$mode" in
    tty)
        unset WAYLAND_DISPLAY DISPLAY WLR_BACKENDS
        exec ./compositor/dwl -s 'sh shell/session.sh' "$@"
        ;;
    nested)
        export WLR_BACKENDS=wayland
        exec ./compositor/dwl -s 'sh shell/session.sh' "$@"
        ;;
    bare)
        exec ./compositor/dwl "$@"
        ;;
    *) printf '%s\n' 'Usage: sh tools/start-session.sh [tty|nested|bare] [dwl arguments]' >&2; exit 2 ;;
esac
