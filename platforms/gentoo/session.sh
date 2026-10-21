# Sourced by shell/session.sh. Gentoo/OpenRC audio startup only.
# Reuse an existing user audio server when one is already running.
if command -v gentoo-pipewire-launcher >/dev/null 2>&1 \
        && command -v wpctl >/dev/null 2>&1 \
        && ! wpctl status >/dev/null 2>&1; then
    # A crashed login can leave old daemons without their runtime sockets.
    # The launcher otherwise refuses to start whenever those PIDs survive.
    if pgrep -u "$(id -u)" -x 'pipewire|wireplumber' >/dev/null 2>&1; then
        gentoo-pipewire-launcher restart &
    else
        gentoo-pipewire-launcher &
    fi
    # Give WirePlumber time to discover the sink before logon sounds.
    for audio_attempt in 1 2 3 4 5; do
        if wpctl get-volume @DEFAULT_AUDIO_SINK@ >/dev/null 2>&1; then break; fi
        sleep 1
    done
    unset audio_attempt
fi
