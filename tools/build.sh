#!/usr/bin/env bash
set -euo pipefail
quickslot_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
quickslot_build="${QUICKSLOT_BUILD_DIR:-$quickslot_root/build/windows-local}"
quickslot_package="${QUICKSLOT_PACKAGE_DIR:-$quickslot_root/build/package}"
cmake -S "$quickslot_root" -B "$quickslot_build" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$quickslot_build" --parallel "${QUICKSLOT_BUILD_JOBS:-4}"
cmake --install "$quickslot_build" --prefix "$quickslot_package"
