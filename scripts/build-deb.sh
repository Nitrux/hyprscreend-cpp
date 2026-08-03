#!/usr/bin/env bash

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2026 <Nitrux Latinoamericana S.C. <hello@nxos.org>>


# -- Exit on errors.

set -euo pipefail


# -- Compile Source

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
mkdir -p build
cd build

PACKAGE_VERSION="${PACKAGE_VERSION:-0.0.1}"

cmake \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    -DCPACK_PACKAGE_VERSION="${PACKAGE_VERSION}" \
    ..

cmake --build . --parallel "$(nproc)"
cpack -G DEB -C Release

