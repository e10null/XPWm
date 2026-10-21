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
# Retain compositor/client diagnostics across an unexpected return to the TTY.
# Restrict access: application diagnostics may contain private paths or URLs.
if [ "$mode" != bare ]; then
    session_log_dir="${XDG_STATE_HOME:-$HOME/.local/state}/xpwm"
    if (umask 077; mkdir -p "$session_log_dir" && touch "$session_log_dir/session.log"); then
        chmod 700 "$session_log_dir"
        chmod 600 "$session_log_dir/session.log"
        if [ -f "$session_log_dir/session.previous.log" ]; then
            rm -f "$session_log_dir/session.previous.log"
        fi
        mv "$session_log_dir/session.log" "$session_log_dir/session.previous.log"
        (umask 077; : > "$session_log_dir/session.log")
        exec 2>>"$session_log_dir/session.log"
        printf '\nXPWm session started: %s\n' "$(date -Iseconds)" >&2
    fi
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
