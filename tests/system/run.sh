#!/bin/bash
# Brings up what a run from outside needs and hands the way to the driver.
#
# The tool takes care of the accessibility bus, the session bus and the
# WebDriver server itself. What it does not bring is a screen: it expects a
# nested compositor, and this image has none, so the screen and a window
# manager are started here. The screen matters because the accessibility bridge
# hangs off the xcb platform and does not load under the offscreen one the
# image otherwise sets; the window manager matters because otherwise no window
# holds the input focus and a synthesised key arrives nowhere.
#
# The way is walked twice, under two locales. The steps name no visible text,
# so both walks hold whatever the interface says.
#
# The second walk shows more than that: with the interface speaking another
# language, an element held by an id is still found while one held by its name
# would be lost. The sample below is what carries that, and it differs between
# the two locales as long as the catalogue holds the menu bar the sample is
# taken from. Where the two come out the same, the walk says so rather than
# passing the pair off as evidence it is not.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

: "${OLBAFLINX_BINARY:?name the executable to drive in OLBAFLINX_BINARY}"
: "${DISPLAY_NUMBER:=99}"
: "${FIRST_LANGUAGE:=en_US.UTF-8}"
: "${SECOND_LANGUAGE:=de_DE.UTF-8}"
: "${APPIUM_PYTHON:=/opt/appium-client/bin/python}"

Xvfb ":${DISPLAY_NUMBER}" -screen 0 1280x1024x24 >/dev/null 2>&1 &
xvfb_pid=$!
export DISPLAY=":${DISPLAY_NUMBER}"

matchbox_pid=""

cleanup() {
    for pid in "$matchbox_pid" "$xvfb_pid"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    done
    rm -f "${first_output:-}" "${second_output:-}"
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

# A session would set this, and a container has none. The launcher creates the
# directory it names and fails on the empty value before it does anything else.
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/runtime-outside}"
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"

# The nested compositor the launcher would otherwise exec itself into. There is
# none in this image, and the run does not need one: it drives through the
# interface, not through the screen. It also decides how the driver types:
# without a compositor it synthesises keys through the accessibility interface
# instead of through the wayland protocol, and that is the way that works on
# the screen this run brings.
export TEST_WITH_KWIN_WAYLAND=0

# The launcher makes a fresh XDG home of its own, which is welcome, but the run
# sets its own anyway so that it walks a first start even when told otherwise.
export QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1

walk() {
    local language="$1"
    local output="$2"

    echo "=== the way, under ${language} ==="
    LANG="$language" LANGUAGE="${language%%.*}" LC_ALL="$language" \
        selenium-webdriver-at-spi-run "$APPIUM_PYTHON" "${here}/first_start.py" \
        | tee "$output"
    echo
}

first_output="$(mktemp)"
second_output="$(mktemp)"

walk "$FIRST_LANGUAGE" "$first_output"
walk "$SECOND_LANGUAGE" "$second_output"

first_sample="$(sed -n 's/^language-sample: //p' "$first_output")"
second_sample="$(sed -n 's/^language-sample: //p' "$second_output")"

echo "=== what the two walks say ==="
echo "the interface spoke \"${first_sample}\" and \"${second_sample}\""
echo "ok    every step held under both locales, and no step named a visible text"
if [ "$first_sample" = "$second_sample" ]; then
    echo "note  the interface said the same both times, so this pair says" \
         "nothing about the ids: the catalogue of the second locale is not" \
         "carrying the text the sample is taken from"
else
    echo "ok    the interface spoke two languages and every element was still" \
         "found, which is what holding an element by its id is for"
fi
