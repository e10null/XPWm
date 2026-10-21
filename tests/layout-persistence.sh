#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
test_runtime=$(mktemp -d /tmp/xpwm-layout-persistence.XXXXXX)
trap 'rm -rf -- "$test_runtime"' EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$test_runtime" XDG_CONFIG_HOME="$test_runtime/config"
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=2 WLR_RENDERER=pixman
unset WAYLAND_DISPLAY DISPLAY
XPWM_EXPECT_FIRST='Floating Mode' XPWM_EXPECT_SECOND='Floating Mode' \
    XPWM_SAVE_FIRST='WM Mode' timeout 10 ./compositor/layout-persistence-test
XPWM_EXPECT_FIRST='WM Mode' XPWM_EXPECT_SECOND='Floating Mode' \
    XPWM_SAVE_FIRST='Floating Mode' XPWM_SAVE_SECOND='WM Mode' \
    timeout 10 ./compositor/layout-persistence-test
XPWM_EXPECT_FIRST='Floating Mode' XPWM_EXPECT_SECOND='WM Mode' \
    timeout 10 ./compositor/layout-persistence-test
# An unseen connector uses the last selected mode, retaining old per-output groups.
python3 - <<'PYTEST'
import configparser, os
path = os.path.join(os.environ['XDG_CONFIG_HOME'], 'xpwm', 'layouts.ini')
c = configparser.ConfigParser()
c.optionxform = str
c.read(path)
for section in list(c.sections()):
    if section.startswith('Output-'):
        c.remove_section(section)
with open(path, 'w') as f:
    c.write(f)
PYTEST
XPWM_EXPECT_FIRST='WM Mode' XPWM_EXPECT_SECOND='WM Mode' \
    timeout 10 ./compositor/layout-persistence-test
printf 'invalid settings file\n' > "$XDG_CONFIG_HOME/xpwm/layouts.ini"
XPWM_EXPECT_FIRST='Floating Mode' XPWM_EXPECT_SECOND='Floating Mode' \
    timeout 10 ./compositor/layout-persistence-test
