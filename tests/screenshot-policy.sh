#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
crop_runtime=$(mktemp -d /tmp/xpwm-crop-policy.XXXXXX)
trap 'rm -rf -- "$crop_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$crop_runtime" XDG_CONFIG_HOME="$crop_runtime/config" XDG_DATA_HOME="$crop_runtime/data"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/screenshot-policy-test
test -s "$crop_runtime/policy-passed"
cat "$crop_runtime/policy-passed"
python3 - <<'PY'
import os, struct
with open(os.path.join(os.environ['XDG_RUNTIME_DIR'],'area.png'),'rb') as f:
    png=f.read(24)
assert png[:8]==b'\x89PNG\r\n\x1a\n'
assert struct.unpack('>II',png[16:24])==(100,60)
PY
