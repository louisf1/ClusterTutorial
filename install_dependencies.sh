#!/bin/bash
# install_dependencies.sh — bootstrap all system dependencies required to build
# and run ClusterWorx (dobuild.sh) on a fresh Debian 12 (bookworm) installation.
#
# This script is idempotent — safe to re-run.
# Discovered incrementally by running dobuild.sh on a clean Debian 12 install
# and installing each missing dependency as it surfaced (cross-checked against
# apt_packages.txt_VER2, the original setup notes).
#
# Does NOT install: Qt6 itself (installed manually via the Qt online installer
# into ~/Qt — see qt-online-installer-linux-x64-*.run in this repo) or
# arduino-cli (expected at ~/bin/arduino-cli — install from
# https://arduino.github.io/arduino-cli/latest/installation/ if missing).
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
sudo apt-get install -y \
    python3-pip \
    python3-venv \
    python3.11-venv

if [ ! -d "$HOME/.conan-env" ]; then
    echo "==> Creating ~/.conan-env venv"
    python3 -m venv "$HOME/.conan-env"
fi
"$HOME/.conan-env/bin/pip" install --upgrade pip -q
"$HOME/.conan-env/bin/pip" install conan kuksa-client -q

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

echo "==> Building and installing kuksaclient (KUKSA C++ client library)"
KUKSA_CPP_DIR="$HOME/ClusterWorx/kuksa-incubation-main/kuksa-cpp-client"
if [ -d "$KUKSA_CPP_DIR" ] && [ ! -f /usr/local/lib/libkuksaclient.so ]; then
    mkdir -p "$KUKSA_CPP_DIR/build"
    (
        cd "$KUKSA_CPP_DIR/build"
        cmake ..
        make -j"$(nproc)"
        sudo make install
    )
fi
sudo mkdir -p /usr/etc/kuksaclient
if [ ! -e /usr/include/kuksaclient ] && [ -d /usr/local/include/kuksaclient ]; then
    sudo ln -s /usr/local/include/kuksaclient /usr/include/kuksaclient
fi
sudo ldconfig

echo "==> All dependencies installed. Run ./dobuild.sh to build and launch ClusterDashboardApp."
echo "    If this is the first install, start a new shell (or 'newgrp docker') before running docker compose."
