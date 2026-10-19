#!/bin/sh
# Run from any directory; dependency changes require an explicit --deps option.
set -eu
installer_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if ! command -v python3 >/dev/null 2>&1; then
    printf '%s\n' 'XPWm installer requires Python 3. Install python3 using your distribution package manager.' >&2
    exit 1
fi
exec python3 "$installer_root/tools/install.py" "$@"
