#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
profile_runtime=$(mktemp -d /tmp/xpwm-layout-test.XXXXXX)
cleanup_profile() {
    # Firefox can activate a private document portal. Let its runtime directory
    # settle after the private bus exits before removing the fixture.
    for cleanup_attempt in 1 2 3 4 5; do
        if rm -rf -- "$profile_runtime" 2>/dev/null; then return; fi
        sleep 0.2
    done
    rm -rf -- "$profile_runtime"
}
trap cleanup_profile EXIT HUP INT TERM
export XDG_RUNTIME_DIR="$profile_runtime" XDG_CONFIG_HOME="$profile_runtime/config"
export XDG_DATA_HOME="$profile_runtime/data" XDG_CACHE_HOME="$profile_runtime/cache"
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$profile_runtime/firefox"
cat > "$profile_runtime/firefox/user.js" <<'PREFS'
user_pref("browser.shell.checkDefaultBrowser", false);
user_pref("browser.aboutwelcome.enabled", false);
user_pref("browser.startup.homepage_override.mstone", "ignore");
user_pref("datareporting.policy.dataSubmissionEnabled", false);
user_pref("toolkit.telemetry.enabled", false);
user_pref("browser.newtabpage.enabled", false);
PREFS
export WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman
export XPWM_SOUND_MUTE=1
unset WAYLAND_DISPLAY DISPLAY DBUS_SESSION_BUS_ADDRESS
timeout 35 dbus-run-session -- ./compositor/layout-state-test
