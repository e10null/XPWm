#!/bin/sh
# Capture VT-switch diagnostics from a real local console.
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
log="$project_root/tty-debug.log"
umask 077
export XPWM_DEBUG_VT=1
printf 'Starting XPWm; diagnostics: %s\nTry Ctrl+Alt+F3, then sign out if it fails.\n' "$log"
exec sh "$project_root/tools/start-session.sh" tty 2> "$log"
