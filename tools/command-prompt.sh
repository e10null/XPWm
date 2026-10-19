#!/bin/sh
# A native, themeable Command Prompt-style terminal; each shortcut opens a window.
set -eu
if command -v xfce4-terminal >/dev/null 2>&1; then
    exec xfce4-terminal --disable-server --title='Command Prompt' --geometry=80x25 \
        --hide-menubar --hide-toolbar --hide-scrollbar \
        --color-bg='#000000' --color-text='#c0c0c0' --font='Monospace 10'
fi
exec foot --title='Command Prompt' --font='monospace:size=10' \
    -o colors.background=000000 -o colors.foreground=c0c0c0
