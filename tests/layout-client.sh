#!/bin/sh
set -eu
. tools/session-theme.sh
GDK_BACKEND=wayland ./shell/layout-client-test &
foot --app-id=xpwm-layout-terminal --title='Layout terminal' sh -c 'sleep 60' &
if [ "${XPWM_LAYOUT_FIREFOX:-}" = 1 ]; then
    MOZ_ENABLE_WAYLAND=1 MOZ_DBUS_REMOTE=0 firefox --no-remote --profile "$XDG_RUNTIME_DIR/firefox" about:blank &
fi
wait
