#!/bin/sh
# xvfb-run supplies X11; a WM is required for focus and fullscreen geometry.
set -eu
"$1" >/dev/null 2>&1 &
neko_wm_pid=$!
trap 'kill "$neko_wm_pid" 2>/dev/null || :; wait "$neko_wm_pid" 2>/dev/null || :' EXIT
shift
"$@"
