#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"
[[ -x configure ]] || ./autogen.sh
mkdir -p build/tui
cd build/tui
[[ -f Makefile ]] || ../../configure --prefix=/usr/local CFLAGS='-O2 -DNDEBUG'
make -j"${HTOP_JOBS:-4}"
mapfile -t objects < <(make -s --no-print-directory --eval='collector-objects:;@echo $(htop_OBJECTS)' collector-objects | tr ' ' '\n' | sed '/^htop\.o$/d;/^$/d')
ar rcs libhtop-collector.a "${objects[@]}"
cd "$root"
cmake -S fltk -B build/gui -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="${DOME_FLTK_PREFIX:-/opt/fltk-vr/toolkit}"
cmake --build build/gui --parallel "${HTOP_JOBS:-4}"
