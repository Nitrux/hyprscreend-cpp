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

HOST_MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH)
PACKAGE_VERSION="${PACKAGE_VERSION:-0.1.0}"

cmake \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_NO_PACKAGE_REGISTRY=ON \
    -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON \
    -DCMAKE_INSTALL_LIBDIR="/usr/lib/${HOST_MULTIARCH}" \
    ..

cmake --build . --parallel "$(nproc)"


# -- Run checkinstall and Build Debian Package

>> description-pak printf "%s\n" \
	'Nitrux Hyprland display and power daemon.' \
	'' \
	'hyprscreend manages monitor modes, scaling, and hotplug configuration.' \
	''

checkinstall -D -y \
	--install=no \
	--fstrans=yes \
	--pkgname=hyprscreend \
	--pkgversion="$PACKAGE_VERSION" \
	--pkgarch="$(dpkg --print-architecture)" \
	--pkgrelease="1" \
	--pkglicense=BSD-3 \
	--pkggroup=utils \
	--pkgsource=hyprscreend \
	--pakdir=. \
	--maintainer=uri_herrera@nxos.org \
	--provides=hyprscreend \
	--requires="hyprland" \
	--nodoc \
	--strip=no \
	--stripso=yes \
	--reset-uids=yes \
	--deldesc=yes \
	cmake --install .
