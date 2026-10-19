#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$project_root"
if [ "${1:-}" = client ]; then
	XPWM_LOGON_AUTO=1 XPWM_SOUND_MUTE=1 XPWM_SESSION_DRY_RUN=1 XPWM_SESSION_MARKER="$XDG_RUNTIME_DIR/logon" \
		GDK_BACKEND=wayland ./shell/xpwm-logon
	test -s "$XDG_RUNTIME_DIR/logon"
	XPWM_LOGOFF_DELAY_MS=80 XPWM_SOUND_MUTE=1 XPWM_SESSION_DRY_RUN=1 XPWM_SESSION_MARKER="$XDG_RUNTIME_DIR/logoff" \
		GDK_BACKEND=wayland ./shell/xpwm-logoff
	test "$(cat "$XDG_RUNTIME_DIR/logoff")" = logoff
	printf '%s\n' 'PASS: XP logon acceptance and logoff transition screens'
	kill -TERM "$PPID"
	exit 0
fi
test_runtime=$(mktemp -d /tmp/xpwm-session-screen.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/dwl -s 'sh tests/session-screen.sh client'
