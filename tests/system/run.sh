#!/bin/bash
# Brings up what a run from outside needs, then walks the way twice.
#
# Four things have to stand before the application starts, and the order is not
# free. The screen comes first, because the accessibility bridge hangs off the
# xcb platform and does not load under the offscreen one the image otherwise
# sets. The accessibility bus comes before the application, because one that
# finds no bus at startup never announces itself afterwards. The window manager
# comes before it as well, because otherwise no window holds the input focus
# and a synthesised key arrives nowhere. And the desktop setting comes before
# the test framework, which refuses to import without it.
#
# The way is walked twice, under two locales. The steps name no visible text,
# so both runs hold whatever the interface says.
#
# The second walk was meant to show more than that: with the interface speaking
# another language, an element held by an id would still be found while one
# held by its name would be lost. It cannot show that yet. Both translation
# files of the project carry every string as unfinished, so there is no second
# language for the interface to speak, and the sample below comes out the same
# twice. What the second walk proves today is that a different locale does not
# disturb the run; the rest of the evidence waits on a translation.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

: "${OLBAFLINX_BINARY:?name the executable to drive in OLBAFLINX_BINARY}"
: "${DISPLAY_NUMBER:=99}"
: "${FIRST_LANGUAGE:=en_US.UTF-8}"
: "${SECOND_LANGUAGE:=de_DE.UTF-8}"

# A session bus of its own. The script re-enters itself under one rather than
# splitting into a second file.
if [ -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]; then
    exec dbus-run-session -- "${BASH_SOURCE[0]}" "$@"
fi

Xvfb ":${DISPLAY_NUMBER}" -screen 0 1280x1024x24 >/dev/null 2>&1 &
xvfb_pid=$!
export DISPLAY=":${DISPLAY_NUMBER}"

matchbox_pid=""
launcher_pid=""

cleanup() {
    for pid in "$matchbox_pid" "$launcher_pid" "$xvfb_pid"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    done
    return 0
}
trap cleanup EXIT

# Waited for rather than slept at: the socket appears when the server is ready.
for _ in $(seq 100); do
    [ -S "/tmp/.X11-unix/X${DISPLAY_NUMBER}" ] && break
    sleep 0.1
done
[ -S "/tmp/.X11-unix/X${DISPLAY_NUMBER}" ] || {
    echo "no X server on :${DISPLAY_NUMBER}" >&2
    exit 1
}

matchbox-window-manager -use_titlebar no >/dev/null 2>&1 &
matchbox_pid=$!

gsettings set org.gnome.desktop.interface toolkit-accessibility true

/usr/libexec/at-spi-bus-launcher --launch-immediately &
launcher_pid=$!

for _ in $(seq 100); do
    dbus-send --session --dest=org.a11y.Bus /org/a11y/bus \
        org.a11y.Bus.GetAddress >/dev/null 2>&1 && break
    sleep 0.1
done
dbus-send --session --dest=org.a11y.Bus /org/a11y/bus \
    org.a11y.Bus.GetAddress >/dev/null 2>&1 || {
    echo "no accessibility bus" >&2
    exit 1
}

# The setting above is what turns the bridge on; this says the same a second
# way and costs nothing.
export QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1

walk() {
    local language="$1"
    local output="$2"

    echo "=== the way, under ${language} ==="
    LANG="$language" LANGUAGE="${language%%.*}" LC_ALL="$language" \
        python3 -u "${here}/first_start.py" | tee "$output"
    echo
}

first_output="$(mktemp)"
second_output="$(mktemp)"
trap 'rm -f "$first_output" "$second_output"; cleanup' EXIT

walk "$FIRST_LANGUAGE" "$first_output"
walk "$SECOND_LANGUAGE" "$second_output"

first_sample="$(sed -n 's/^language-sample: //p' "$first_output")"
second_sample="$(sed -n 's/^language-sample: //p' "$second_output")"

echo "=== what the two walks say ==="
echo "the interface spoke \"${first_sample}\" and \"${second_sample}\""
echo "ok    every step held under both locales, and no step named a visible text"
if [ "$first_sample" = "$second_sample" ]; then
    echo "note  the interface said the same both times: the project translates" \
         "no second language yet, so this pair says nothing about the ids"
fi
