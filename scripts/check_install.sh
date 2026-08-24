#!/usr/bin/env bash
#
# assert the installed header-tree ships no cereal. serialization lives under src/, so this should
# hold by construction - this is the regression-guard, not the mechanism.
#
# usage: check_install.sh <build-dir>

set -euo pipefail

build_dir="${1:?usage: check_install.sh <build-dir>}"
prefix="$(mktemp -d)"
trap 'rm -rf "${prefix}"' EXIT

cmake --install "${build_dir}" --prefix "${prefix}" > /dev/null

if hits=$(grep -rl "cereal/" "${prefix}/include" 2>/dev/null); then
    echo "error: cereal reached the install-tree:" >&2
    echo "${hits}" >&2
    exit 1
fi

echo "install-tree is cereal-free"
