#!/usr/bin/env bash
set -euo pipefail

# Build and install the Sitronix ST7703 DRM panel driver as an out-of-tree module
# for the currently running Raspberry Pi kernel.

if [[ $EUID -ne 0 ]]; then
  echo "Please run as root (sudo)." >&2
  exit 1
fi

TARGET_KVER="${TARGET_KVER:-}"
KVER="${TARGET_KVER:-$(uname -r)}"
ARCH="$(uname -m)"
WORKDIR="/tmp/st7703_build"
SRC_DIR="$WORKDIR/rpi-linux"
EXTMOD_DIR="$WORKDIR/st7703_extmod"
KO_PATH="$EXTMOD_DIR/panel-sitronix-st7703.ko"
MODEL_FILE="/proc/device-tree/model"
MODEL="$(tr -d '\0' < "$MODEL_FILE" 2>/dev/null || echo unknown)"
CURRENT_KVER="$(uname -r)"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OVERLAY_SRC="$PROJECT_ROOT/st7703-overlay.dts"
PKGROOT="$WORKDIR/pkgroot"

echo "Kernel target: $KVER ($ARCH)"
echo "Model: ${MODEL}"

echo "Installing build prerequisites..."
apt-get update -y
apt-get install -y bc bison flex libssl-dev make git ca-certificates apt-transport-https

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"
cd "$WORKDIR"

# Choose a branch that matches the running kernel ABI as closely as possible.
# For RPi kernels, this typically tracks rpi-<major>.<minor>. We'll default to rpi-6.12.y.
BRANCH="rpi-6.12.y"
REPO="https://github.com/raspberrypi/linux.git"

echo "Cloning Raspberry Pi kernel ($BRANCH)..."
git clone --depth 1 --branch "$BRANCH" "$REPO" "$SRC_DIR"

cd "$SRC_DIR"

echo "Preparing Kbuild for external build against running kernel..."
export KERNELRELEASE="$KVER"

# Locate or fetch headers for the target kernel version
KERNELDIR="/lib/modules/$KVER/build"
if [[ ! -d "$KERNELDIR" ]]; then
  echo "Headers for $KVER not installed locally. Fetching raspberrypi-kernel-headers package..."
  HDRS_DL_DIR="$WORKDIR/headers"
  mkdir -p "$HDRS_DL_DIR"
  apt-get download raspberrypi-kernel-headers
  dpkg-deb -x raspberrypi-kernel-headers_*.deb "$HDRS_DL_DIR"
  # Attempt to find a headers dir matching the exact target KVER
  if [[ -d "$HDRS_DL_DIR/usr/src/linux-headers-$KVER" ]]; then
    KERNELDIR="$HDRS_DL_DIR/usr/src/linux-headers-$KVER"
  else
    # Heuristic: pick arch-specific directory that matches suffix of KVER
    SUFFIX="${KVER##*+rpt-}"
    CANDIDATE="$(ls -d $HDRS_DL_DIR/usr/src/linux-headers-* 2>/dev/null | grep -E "$SUFFIX$" | head -n1 || true)"
    if [[ -n "$CANDIDATE" && -d "$CANDIDATE" ]]; then
      KERNELDIR="$CANDIDATE"
    else
      # Fallback to common headers
      if [[ -d "$HDRS_DL_DIR/usr/src/linux-headers-${KVER%%+*}+${KVER#*+}-common-rpi" ]]; then
        KERNELDIR="$HDRS_DL_DIR/usr/src/linux-headers-${KVER%%+*}+${KVER#*+}-common-rpi"
      else
        # Pick any headers dir as last resort
        KERNELDIR="$(ls -d $HDRS_DL_DIR/usr/src/linux-headers-* 2>/dev/null | head -n1)"
      fi
    fi
  fi
  if [[ -z "$KERNELDIR" || ! -d "$KERNELDIR" ]]; then
    echo "Failed to locate headers for $KVER in downloaded package." >&2
    exit 2
  fi
fi

echo "Using KERNELDIR: $KERNELDIR"

echo "Preparing external module source..."
mkdir -p "$EXTMOD_DIR"
cp "$SRC_DIR/drivers/gpu/drm/panel/panel-sitronix-st7703.c" "$EXTMOD_DIR/"
cat > "$EXTMOD_DIR/Makefile" << 'MK'
obj-m := panel-sitronix-st7703.o
MK

echo "Building panel-sitronix-st7703 module (external Kbuild)..."
make -C "$KERNELDIR" M="$EXTMOD_DIR" modules

if [[ ! -f "$KO_PATH" ]]; then
  echo "Build failed: $KO_PATH not found" >&2
  exit 3
fi

if [[ "$KVER" == "$CURRENT_KVER" ]]; then
  echo "Installing module into live system (kernel matches target)..."
  install -D -m 0644 "$KO_PATH" \
    "/lib/modules/$KVER/extra/panel-sitronix-st7703.ko"
  depmod -a "$KVER"
  if [[ -f "$OVERLAY_SRC" ]]; then
    echo "Building and installing overlay..."
    dtc -@ -I dts -O dtb -o "$WORKDIR/st7703.dtbo" "$OVERLAY_SRC"
    install -D -m 0644 "$WORKDIR/st7703.dtbo" \
      "/boot/firmware/overlays/st7703.dtbo"
  fi
  echo "Loading module..."
  modprobe panel-sitronix-st7703 || true
  echo "Verifying..."
  modinfo panel-sitronix-st7703 || true
else
  echo "Cross-build detected (host=$CURRENT_KVER target=$KVER). Packaging for transfer..."
  mkdir -p "$PKGROOT"
  install -D -m 0644 "$KO_PATH" \
    "$PKGROOT/lib/modules/$KVER/extra/panel-sitronix-st7703.ko"
  if [[ -f "$OVERLAY_SRC" ]]; then
    dtc -@ -I dts -O dtb -o "$WORKDIR/st7703.dtbo" "$OVERLAY_SRC"
    install -D -m 0644 "$WORKDIR/st7703.dtbo" \
      "$PKGROOT/boot/firmware/overlays/st7703.dtbo"
  fi
  PKG_TGZ="$WORKDIR/st7703-$KVER.tgz"
  tar -C "$PKGROOT" -czf "$PKG_TGZ" .
  echo "Package ready: $PKG_TGZ"
  echo
  echo "Install on target (CM4) commands:"
  echo "  scp -3 admin@pi-01:$PKG_TGZ admin@nfc-terminal:/tmp/st7703-$KVER.tgz"
  echo "  ssh admin@nfc-terminal 'sudo tar -C / -xzf /tmp/st7703-$KVER.tgz && sudo depmod -a $KVER'"
  echo "  ssh admin@nfc-terminal 'sudo modprobe panel-sitronix-st7703 || true'"
fi

echo
echo "Done. If the overlay is correct, check dmesg for DSI/panel probe messages."
if echo "$MODEL" | grep -q "Raspberry Pi 5"; then
  echo "Pi 5 detected: use 'dtoverlay=vc4-kms-v3d-pi5' plus your overlay."
else
  echo "Pi 4/CM4 detected: use 'dtoverlay=vc4-kms-v3d' plus your overlay."
fi
 
