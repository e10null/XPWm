#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" = client ]; then
 PATH="$XDG_RUNTIME_DIR/bin" GDK_BACKEND=wayland XPWM_SOUND_MUTE=1 ./shell/run-completion-test
 kill -TERM "$PPID"
 exit 0
fi
run_runtime=$(mktemp -d /tmp/xpwm-run-completion.XXXXXX)
trap 'rm -rf -- "$run_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$run_runtime" XDG_CONFIG_HOME="$run_runtime/config" XDG_DATA_HOME="$run_runtime/data"
mkdir -p "$run_runtime/bin"
printf '#!/bin/sh\nprintf apple > "$XDG_RUNTIME_DIR/launched"\n' > "$run_runtime/bin/xpwm-apple"
printf '#!/bin/sh\nprintf apricot > "$XDG_RUNTIME_DIR/launched"\n' > "$run_runtime/bin/xpwm-apricot"
chmod +x "$run_runtime/bin/"*
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
timeout 15 ./compositor/dwl -s 'sh tests/run-completion.sh client'

test -s "$run_runtime/passed"
cat "$run_runtime/passed"
