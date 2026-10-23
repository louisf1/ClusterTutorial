#!/bin/bash
# install_dependencies.sh — bootstrap all system dependencies required to build
# and run ClusterWorx (dobuild.sh) on this board's Rockchip vendor image
# (Debian 11 bullseye + ~100 apt-held vendor packages). Includes a surgical
# glibc/GCC-12 partial upgrade from bookworm — see the [FIX] block below.
#
# This script is idempotent — safe to re-run.
# Discovered incrementally by running dobuild.sh on a clean Debian 12 install
# and installing each missing dependency as it surfaced (cross-checked against
# apt_packages.txt_VER2, the original setup notes).
#
# Installs Qt 6.4.2 arm64 from Debian bookworm packages (see the Qt [FIX]
# block below for why no official Qt binary distribution works on this board).
#
# Also installs arduino-cli to ~/bin/arduino-cli (+ arduino:avr core).
#
# Usage: ./install_dependencies.sh

set -e

if [ "$(id -u)" -eq 0 ]; then
    echo "Do not run this script as root — it uses sudo internally." >&2
    exit 1
fi

echo "==> Fixing apt sources (fresh installs point at the install cdrom, which breaks apt update)"
sudo sed -i '/^deb cdrom:/s/^/# /' /etc/apt/sources.list

echo "==> apt-get update"
sudo apt-get update -qq

echo "==> Base build/dev tools"
sudo apt-get install -y \
    git \
    build-essential \
    cmake \
    ninja-build \
    pkg-config

echo "==> OpenGL / Wayland / XKB / Vulkan / XCB (Qt6 GUI build + runtime deps)"
sudo apt-get install -y \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libegl1-mesa-dev \
    libwayland-dev \
    libwayland-egl-backend-dev \
    wayland-protocols \
    libxkbcommon-dev \
    libxkbcommon-x11-dev \
    libvulkan-dev \
    libxcb-cursor0 \
    libglib2.0-0

echo "==> gRPC / Protobuf / TLS / logging (kuksaclient + ClusterDashboardApp deps)"
sudo apt-get install -y \
    libssl-dev \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    libprotobuf-dev \
    libspdlog-dev \
    libfmt-dev \
    libfuse2

echo "==> Python (Conan + kuksa-client run in a venv to avoid PEP 668 conflicts)"
# [FIX] The versioned venv package must match the installed python3 (3.9 on
#       the Rockchip bullseye image, 3.13 on Raspberry Pi OS trixie, ...).
#       Hardcoding python3.9-venv made apt abort on any other distro.
PY_VER="$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')"
PY_PKGS="python3-pip python3-venv"
if apt-cache show "python${PY_VER}-venv" >/dev/null 2>&1; then
    PY_PKGS="$PY_PKGS python${PY_VER}-venv"
fi
sudo apt-get install -y $PY_PKGS

# [FIX] Debian 11's apt cmake is 3.18.4, but Qt 6.10.3 refuses to configure
#       with anything older than 3.22 ("CMake 3.22 or higher is required to
#       use Qt"). Install a newer CMake via pip into ~/.local/bin, which
#       dobuild.sh prepends onto PATH ahead of the system one.
#       Skipped when the system cmake is already new enough (e.g. trixie's
#       3.31) — a system-wide pip install would fail there anyway (PEP 668).
if ! printf '3.22\n%s\n' "$(cmake --version | awk 'NR==1{print $3}')" | sort -V -C; then
    echo "==> Installing CMake >=3.22 via pip (system apt cmake is too old for Qt 6.10.3)"
    #python3 -m pip install --user --upgrade "cmake>=3.22"
    python3 -m pip install --upgrade "cmake>=3.22"
fi

# [FIX] The official Qt arm64 binaries need glibc >=2.35 and GLIBCXX >=3.4.30;
#       this Rockchip vendor image is Debian 11 (glibc 2.31, GCC 10). A full
#       bullseye->bookworm upgrade is IMPOSSIBLE here — ~100 vendor packages
#       (Rockchip display/video stack) are apt-held and pin old core libs.
#       Instead: a surgical partial upgrade of ONLY glibc + libstdc++ + the
#       GCC 12 toolchain from bookworm (both are backward-ABI-compatible, so
#       every vendor binary keeps working). Verified via apt simulation:
#       49 installs, 0 removals, no vendor runtime package touched.
if ! ldd --version | head -1 | grep -qE " 2\.(3[5-9]|[4-9][0-9])"; then
    echo "==> Partial upgrade: glibc/libstdc++/GCC 12 from bookworm (Qt arm64 needs glibc >=2.35)"

    # These vendor -dev packages (build-time only, no runtime impact) pin old
    # libc6-dev/libglib2.0-dev and make the resolver fail — remove them first.
    sudo apt-mark unhold libgstreamer1.0-dev libgstreamer1.0-dev-dbgsym \
        libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev \
        xserver-xorg-dev 2>/dev/null || true
    sudo apt-get remove -y libgstreamer1.0-dev libgstreamer1.0-dev-dbgsym \
        libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev \
        xserver-xorg-dev 2>/dev/null || true

    # bookworm sources, pinned to priority 100 so packages are only ever pulled
    # from it when explicitly requested (pkg/bookworm) — no accidental drift.
    sudo tee /etc/apt/sources.list.d/bookworm.list > /dev/null <<'BOOKWORM'
# Added for ClusterWorx: Qt arm64 binaries need glibc>=2.35 / GLIBCXX>=3.4.30.
# Pinned to priority 100 in /etc/apt/preferences.d/bookworm — packages are only
# pulled from bookworm when explicitly requested (apt install pkg/bookworm).
deb http://deb.debian.org/debian/ bookworm main contrib non-free-firmware
deb http://security.debian.org/debian-security bookworm-security main contrib non-free-firmware
deb http://deb.debian.org/debian/ bookworm-updates main contrib non-free-firmware
BOOKWORM
    sudo tee /etc/apt/preferences.d/bookworm > /dev/null <<'PIN'
Package: *
Pin: release n=bookworm
Pin-Priority: 100

Package: *
Pin: release n=bookworm-security
Pin-Priority: 100

Package: *
Pin: release n=bookworm-updates
Pin-Priority: 100
PIN
    sudo apt-get update -qq

    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
        -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" \
        libc6/bookworm libc6-dev/bookworm libc-bin/bookworm libc-l10n/bookworm \
        locales/bookworm libstdc++6/bookworm libgcc-s1/bookworm gcc-12/bookworm \
        g++-12/bookworm binutils/bookworm dpkg-dev/bookworm \
        build-essential/bookworm gcc/bookworm g++/bookworm
fi

# [FIX] Qt itself: Debian bookworm's arm64 Qt 6.4.2 packages, NOT the official
#       Qt binaries. Chain of failures that led here:
#         - ~/Qt/6.10.3/gcc_64 (online installer) is the x86-64 build — its
#           binaries cannot execute on this aarch64 board at all.
#         - Official arm64 Qt >=6.9 needs glibc >=2.38 / GLIBCXX_3.4.32
#           (Ubuntu 24.04 baseline) — beyond what this vendor image can reach.
#         - Official arm64 Qt 6.8.3 is inconsistent: rcc/uic run, but moc
#           demands GLIBC_2.38 and qmake/qmlimportscanner segfault in
#           std::ostream even with all symbol versions satisfied.
#       Debian's own Qt 6.4.2 arm64 packages are built for exactly the glibc
#       installed above and are fully apt-managed. Runtime libs that bookworm
#       Qt needs (libinput10, libx11, libwayland) are ABI-stable soname
#       upgrades; verified via apt simulation: no vendor package touched.
echo "==> Installing Qt 6.4.2 arm64 from bookworm packages"
QT_BOOKWORM_PKGS="qt6-base-dev qt6-base-private-dev qt6-declarative-dev \
    qt6-declarative-private-dev qt6-wayland-dev qt6-wayland-dev-tools \
    qt6-5compat-dev qt6-quicktimeline-dev qt6-wayland \
    qml6-module-qtquick qml6-module-qtquick-timeline \
    qml6-module-qt5compat-graphicaleffects qml6-module-qtqml \
    qml6-module-qtqml-workerscript qml6-module-qtquick-window \
    qml6-module-qtquick-layouts qml6-module-qtquick-controls \
    qml6-module-qtquick-templates qml6-module-qtquick-shapes \
    libinput10 libx11-xcb1 libx11-6 libwayland-client0 libwayland-cursor0 \
    libwayland-egl1 libwayland-server0 libwayland-dev libinput-dev \
    libx11-dev libwacom-dev libglib2.0-dev libglib2.0-dev-bin libglib2.0-bin \
    libglib2.0-0 libgudev-1.0-dev"
# [FIX] Only pin to /bookworm when the bookworm source was added above
#       (bullseye vendor image). On bookworm/trixie (e.g. Raspberry Pi OS),
#       install the distro's own Qt 6 packages directly.
QT_BOOKWORM_LIST=""
for _pkg in $QT_BOOKWORM_PKGS; do
    if [ -f /etc/apt/sources.list.d/bookworm.list ]; then
        QT_BOOKWORM_LIST="$QT_BOOKWORM_LIST $_pkg/bookworm"
    else
        QT_BOOKWORM_LIST="$QT_BOOKWORM_LIST $_pkg"
    fi
done
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
    -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" \
    $QT_BOOKWORM_LIST

if [ ! -d "$HOME/.conan-env" ]; then
    echo "==> Creating ~/.conan-env venv"
    python3 -m venv "$HOME/.conan-env"
fi
"$HOME/.conan-env/bin/pip" install --upgrade pip -q
"$HOME/.conan-env/bin/pip" install conan kuksa-client pyserial -q

if ! grep -q '.conan-env/bin' "$HOME/.bashrc" 2>/dev/null; then
    echo 'export PATH="$HOME/.conan-env/bin:$PATH"' >> "$HOME/.bashrc"
fi

echo "==> Docker Engine (runs the KUKSA databroker container)"
if ! command -v docker >/dev/null 2>&1; then
    sudo apt-get install -y ca-certificates curl gnupg
    sudo install -m 0755 -d /etc/apt/keyrings
    if [ ! -f /etc/apt/keyrings/docker.gpg ]; then
        curl -fsSL https://download.docker.com/linux/debian/gpg | \
            sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
        sudo chmod a+r /etc/apt/keyrings/docker.gpg
    fi
    echo \
      "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] \
      https://download.docker.com/linux/debian \
      $(lsb_release -cs) stable" | \
      sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
    sudo apt-get update -qq
    sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
    sudo usermod -aG docker "$USER"
    echo "NOTE: log out/in (or run 'newgrp docker') for the docker group membership to take effect."
fi
sudo systemctl enable --now docker

echo "==> arduino-cli + AVR core (dobuild.sh uploads the Mega sketch found by USB ID)"
# dobuild.sh expects arduino-cli at ~/bin/arduino-cli. The official installer
# picks the right binary for the arch (ARM64 on Pi / Rockchip, x86-64 on PC).
if [ ! -x "$HOME/bin/arduino-cli" ]; then
    mkdir -p "$HOME/bin"
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | \
        BINDIR="$HOME/bin" sh
fi
"$HOME/bin/arduino-cli" core update-index
if ! "$HOME/bin/arduino-cli" core list | grep -q '^arduino:avr '; then
    "$HOME/bin/arduino-cli" core install arduino:avr
fi
# Serial port access for uploads + the Python bridge (takes effect on next login).
if ! id -nG "$USER" | grep -qw dialout; then
    sudo usermod -aG dialout "$USER"
fi

echo "==> Building and installing kuksaclient (KUKSA C++ client library)"
# Tracked in the repo (kuksa-incubation-main/ is gitignored, so fresh clones
# never had the sources there).
KUKSA_CPP_DIR="$HOME/ClusterWorx/kuksa-cpp-client"

# [FIX] Upstream kuksa-cpp-client ships a broken CMake config template:
#       cmake/kuksaclientConfig.cmake.in lacks the @PACKAGE_INIT@ marker that
#       configure_package_config_file() expands into the preamble defining
#       PACKAGE_PREFIX_DIR and the set_and_check()/check_required_components()
#       macros. The installed kuksaclientConfig.cmake then resolves the include
#       dir to "/include/kuksaclient" (empty prefix) and every consumer's
#       find_package(kuksaclient) dies with:
#         "File or directory /include/kuksaclient referenced by variable
#          KUKSACLIENT_INCLUDE_DIR does not exist !"
#       Also: set_and_check() on the sysconfig dir hard-fails because
#       /usr/local/etc/kuksaclient is never created by the install — use a
#       plain set() for it instead (nothing consumes it at configure time).
#       Patch the template BEFORE building so the generated config is correct.
if [ -f "$KUKSA_CPP_DIR/cmake/kuksaclientConfig.cmake.in" ] && \
   ! grep -q '@PACKAGE_INIT@' "$KUKSA_CPP_DIR/cmake/kuksaclientConfig.cmake.in"; then
    echo "==> Patching kuksaclientConfig.cmake.in (missing @PACKAGE_INIT@ preamble)"
    sed -i \
        -e 's/^set(KUKSACLIENT_VERSION .*/&\n\n@PACKAGE_INIT@/' \
        -e 's/^set_and_check(KUKSACLIENT_SYSCONFIG_DIR/set(KUKSACLIENT_SYSCONFIG_DIR/' \
        "$KUKSA_CPP_DIR/cmake/kuksaclientConfig.cmake.in"
fi

if [ -d "$KUKSA_CPP_DIR" ] && [ ! -f /usr/local/lib/libkuksaclient.so ]; then
    mkdir -p "$KUKSA_CPP_DIR/build"
    (
        cd "$KUKSA_CPP_DIR/build"
        # kuksa-cpp-client also does find_package(gRPC REQUIRED) but ships no
        # FindgRPC.cmake of its own; point it at ClusterDashboard's copy
        # (same Debian libgrpc-dev-has-no-CMake-config issue as the main build).
        # src/generate.cmake passes ${_gRPC_CPP_PLUGIN_EXECUTABLE} to protoc,
        # a variable only upstream gRPC's CMake package defines — without it
        # protoc gets "--plugin=protoc-gen-grpc=" and fails. Set it explicitly.
        cmake -DCMAKE_MODULE_PATH="$HOME/ClusterWorx/cmake" \
              -D_gRPC_CPP_PLUGIN_EXECUTABLE="$(command -v grpc_cpp_plugin)" ..
        make -j"$(nproc)"
        sudo make install
    )
fi
# [FIX] Repair an already-installed broken kuksaclientConfig.cmake in place.
#       The build block above is skipped when libkuksaclient.so already exists,
#       so a config installed from the unpatched template would never get fixed
#       by re-running this script. Detect the missing preamble (no
#       PACKAGE_PREFIX_DIR definition) and overwrite with a known-good config.
KUKSA_CFG=/usr/local/lib/cmake/kuksaclient/kuksaclientConfig.cmake
if [ -f "$KUKSA_CFG" ] && ! grep -q 'get_filename_component(PACKAGE_PREFIX_DIR' "$KUKSA_CFG"; then
    echo "==> Repairing broken $KUKSA_CFG (missing PACKAGE_PREFIX_DIR preamble)"
    sudo tee "$KUKSA_CFG" > /dev/null <<'EOF'
# Repaired by install_dependencies.sh — upstream template lacked @PACKAGE_INIT@,
# leaving PACKAGE_PREFIX_DIR undefined. This is the equivalent expanded config.
get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

set(KUKSACLIENT_VERSION 0.0.1)

set_and_check(KUKSACLIENT_INCLUDE_DIR "${PACKAGE_PREFIX_DIR}/include/kuksaclient")
# Plain set() — the sysconfig dir may not exist until runtime
set(KUKSACLIENT_SYSCONFIG_DIR "${PACKAGE_PREFIX_DIR}/etc/kuksaclient")

check_required_components(kuksaclient)
EOF
fi

sudo mkdir -p /usr/local/etc/kuksaclient
if [ ! -e /usr/include/kuksaclient ] && [ -d /usr/local/include/kuksaclient ]; then
    sudo ln -s /usr/local/include/kuksaclient /usr/include/kuksaclient
fi
sudo ldconfig

echo "==> All dependencies installed. Run ./dobuild.sh to build and launch ClusterDashboardApp."
echo "    If this is the first install, start a new shell (or 'newgrp docker') before running docker compose."
