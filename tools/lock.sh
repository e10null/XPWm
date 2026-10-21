#!/bin/sh
# swaylock uses ext-session-lock-v1 and the system PAM authentication policy.
# Keep it in the foreground: successful spawn is not proof the session locked.
set -eu
if ! command -v swaylock >/dev/null 2>&1; then
    printf '%s\n' 'XPWm: secure locking requires swaylock. On Gentoo run: doas emerge --ask gui-apps/swaylock' >&2
    exit 1
fi
project_root=${XPWM_PROJECT_ROOT:-$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)}
if [ -x "$project_root/shell/xpwm-lock" ]; then
    exec "$project_root/shell/xpwm-lock" --config /dev/null --ignore-empty-password
fi
exec swaylock --config /dev/null --color 004e98 --ignore-empty-password
