#!/usr/bin/env bash
#
# assert the installed header-tree ships no cereal. keeping serialization out of the install-set is
# what the vierkant/vierkant_projects repo-split used to guarantee structurally.
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

if [ -d "${prefix}/include/vierkant/serialization" ]; then
    echo "error: include/vierkant/serialization was installed" >&2
    exit 1
fi

echo "install-tree is cereal-free"
