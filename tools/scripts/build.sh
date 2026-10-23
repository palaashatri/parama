#!/usr/bin/env bash
# Builds BRAHMA ELF + checksummed Limine x86-64 UEFI/BIOS hybrid ISO.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"
for bin in cmake ninja clang ld.lld curl tar sha256sum xorriso cc make; do
    command -v "$bin" >/dev/null || { echo "[BUILD][ERROR] Missing prerequisite: $bin" >&2; exit 2; }
done
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$ROOT/toolchain/x86_64-none.cmake"
cmake --build "$ROOT/build" --parallel
# Pin and verify the upstream binary distribution. Never execute unverified downloads.
LIMINE_VERSION=12.9.3
LIMINE_SHA256=9f42fe9ea2e84d71056529969b1d5c24ba9a2287ffcc01bcce58b0ef8aa98b1d
ARCHIVE="$ROOT/build/limine-binary-v${LIMINE_VERSION}.tar.gz"
LIMINE_DIR="$ROOT/build/limine-binary"
if [[ ! -f "$ARCHIVE" ]]; then
    curl --fail --location --retry 3 --output "$ARCHIVE.tmp" \
      "https://github.com/limine-bootloader/limine/releases/download/v${LIMINE_VERSION}/limine-binary.tar.gz"
    mv "$ARCHIVE.tmp" "$ARCHIVE"
fi
printf '%s  %s\n' "$LIMINE_SHA256" "$ARCHIVE" | sha256sum --check --status || {
    echo "[BUILD][ERROR] Limine SHA-256 mismatch; remove $ARCHIVE and retry" >&2
    exit 3
}
if [[ ! -d "$LIMINE_DIR" ]]; then
    tar -tzf "$ARCHIVE" > "$ROOT/build/limine-archive-list.txt"
    grep -Eq '^limine-binary/' "$ROOT/build/limine-archive-list.txt" || { echo "Invalid Limine archive layout" >&2; exit 4; }
    tar -xzf "$ARCHIVE" -C "$ROOT/build"
fi
if [[ ! -x "$LIMINE_DIR/limine" ]]; then
    make -C "$LIMINE_DIR" -j2
fi
for file in limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin BOOTX64.EFI limine; do
    [[ -f "$LIMINE_DIR/$file" ]] || { echo "Limine missing $file" >&2; exit 5; }
done
STAGE="$ROOT/build/iso_root"
rm -rf -- "$STAGE"
mkdir -p "$STAGE/boot/limine" "$STAGE/EFI/BOOT" "$STAGE/boot"
cp "$ROOT/build/brahma.elf" "$STAGE/boot/brahma.elf"
cp "$ROOT/boot/limine.conf" "$STAGE/boot/limine/limine.conf"
cp "$LIMINE_DIR/limine-bios.sys" "$LIMINE_DIR/limine-bios-cd.bin" \
   "$LIMINE_DIR/limine-uefi-cd.bin" "$STAGE/boot/limine/"
cp "$LIMINE_DIR/BOOTX64.EFI" "$STAGE/EFI/BOOT/BOOTX64.EFI"
ISO="$ROOT/build/parama.iso"
xorriso -as mkisofs -R -r -J \
    -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 \
    -boot-info-table -hfsplus -apm-block-size 2048 \
    --efi-boot boot/limine/limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    -o "$ISO" "$STAGE"
"$LIMINE_DIR/limine" bios-install "$ISO"
printf '[BUILD] ISO ready: %s\n' "$ISO"
printf '[BUILD] ISO SHA256: '; sha256sum "$ISO" | cut -d' ' -f1
