#!/bin/sh
set -eu
CORE=/usr/libexec/atena/atena-core
UI=/usr/libexec/atena/atena-ui-bin
IDENTITY=/usr/share/atena/identity

fail() {
    msg="$1"
    printf 'Atena: %s\n' "$msg" >&2
    if command -v zenity >/dev/null 2>&1 && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
        zenity --error --title='Atena' --text="$msg" >/dev/null 2>&1 || true
    elif command -v xmessage >/dev/null 2>&1 && [ -n "${DISPLAY:-}" ]; then
        xmessage -center "Atena: $msg" >/dev/null 2>&1 || true
    fi
    exit 127
}

[ -x "$CORE" ] || fail "Core ausente em $CORE. Reinstale o pacote atena_0.4.2-3_amd64.deb."
[ -x "$UI" ] || fail "Interface Qt ausente em $UI. Reinstale o pacote Atena."
[ -r "$IDENTITY/persona.json" ] || fail "Identidade da Atena ausente em $IDENTITY."

export ATENA_CORE_EXECUTABLE="$CORE"
export ATENA_IDENTITY_DIR="$IDENTITY"
exec "$UI" "$@"
