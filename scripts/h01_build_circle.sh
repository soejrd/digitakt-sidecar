#!/bin/bash
# Prepare a Pi 4 Circle boot folder; never formats or writes a physical disk.
# Requires pinned deps plus patches 0001 and 0002 applied (see H01 report).
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
src=$root/deps/sidecar-firmware
out=$root/private/h01/sd
mkdir -p "$out"
docker info >/dev/null
docker image inspect rytm-circle >/dev/null 2>&1 || docker build -t rytm-circle "$src/pi/circle"
# Several upstream setup scripts split source lists on spaces. Use a temporary
# logical path for their setup, while all generated sources remain in deps/.
alias_root=$(mktemp -d /tmp/dt-h01-build.XXXXXX)
trap 'rm -rf "$alias_root"' EXIT
ln -s "$src" "$alias_root/src"
"$alias_root/src/core/engines/setup_engines.sh" "$src/pi/engine/engines"
docker run --rm -v "$src:/repo" -w /repo rytm-circle bash -euo pipefail -c '
base=pi/circle/build/circle
if [ ! -d "$base" ]; then
 git clone --depth 1 --branch Step51.1.1 https://github.com/rsta2/circle.git "$base"
fi
test "$(git -C "$base" rev-parse HEAD)" = 7a81e1b480fe7d19fc1fc695b89bff0a0848f44b
cd "$base"
grep -q RytmObDeviceCreate lib/usb/usbdevicefactory.cpp || patch -p1 < ../../spike2/circle-usbdevicefactory.patch
grep -q XHCI_EP_MAX_URBS include/circle/usb/xhciendpoint.h || patch -p1 < ../../spike2/circle-xhci-urbqueue.patch
./configure -f -r 4 -p aarch64-none-elf-
./makeall --nosample -j4
make -C boot armstub64
# The upstream engine script copies this tree before its first hostap fetch.
if [ ! -f addon/wlan/hostap/wpa_supplicant/Makefile.circle ]; then
 git clone https://github.com/rsta2/hostap.git addon/wlan/hostap
 git -C addon/wlan/hostap checkout 966cab4a3bc958e7ff304a46d91a6172a1bfecaf
fi
'
"$src/pi/engine/build.sh"
cp "$src/pi/engine/kernel8-rpi4.img" "$out/"
cp "$src/pi/circle/build/circle/boot/armstub8-rpi4.bin" "$out/"
fw=0641c5bf25d185d5bf25d8dcafd1da6dc8fdbf68
for file in start4.elf fixup4.dat bcm2711-rpi-4-b.dtb LICENCE.broadcom; do
 curl -fL --retry 2 -o "$out/$file" "https://raw.githubusercontent.com/raspberrypi/firmware/$fw/boot/$file"
done
cat > "$out/config.txt" <<'EOF'
[all]
arm_64bit=1
kernel_address=0x80000
initial_turbo=0
[pi4]
armstub=armstub8-rpi4.bin
kernel=kernel8-rpi4.img
max_framebuffers=2
hdmi_force_hotplug=1
hdmi_group=2
hdmi_mode=87
hdmi_cvt=800 480 60 6 0 0 0
disable_overscan=1
EOF
printf 'out=main notes=off gadget=0 wifi=0 ip=%s netmask=255.255.255.0 web=1 sdport=0 selftest=0 bench=0 um=0\n' "${H01_PI_IP:?set H01_PI_IP to an unused LAN address for Circle}" > "$out/cmdline.txt"
(cd "$out" && shasum -a 256 LICENCE.broadcom armstub8-rpi4.bin bcm2711-rpi-4-b.dtb cmdline.txt config.txt fixup4.dat kernel8-rpi4.img start4.elf > SHA256SUMS)
echo "Prepared $out; review before copying to a separately authorized FAT32/MBR card."
