#!/usr/bin/env bash
# Packages build/brahma.elf into a hybrid BIOS/UEFI Limine ISO.
# Usage: make_iso.sh <output.iso> [WIDTHxHEIGHT]
# The optional resolution adds a Limine `resolution:` line to the boot entry.
# Limine silently falls back if the mode is unavailable, so QA must compare the
# kernel's "[BRAHMA] Framebuffer:" serial line against the requested mode.
# Requires build.sh to have fetched and verified Limine into build/limine-binary.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:?usage: make_iso.sh <output.iso> [WIDTHxHEIGHT]}"
RESOLUTION="${2:-}"
LIMINE_DIR="$ROOT/build/limine-binary"
if [[ -n "$RESOLUTION" && ! "$RESOLUTION" =~ ^[0-9]+x[0-9]+$ ]]; then
    echo "[ISO][ERROR] Resolution must look like 800x600, got: $RESOLUTION" >&2; exit 2
fi
[[ -f "$ROOT/build/brahma.elf" ]] || { echo "[ISO][ERROR] build/brahma.elf missing; run build.sh" >&2; exit 2; }
for file in limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin BOOTX64.EFI limine; do
    [[ -f "$LIMINE_DIR/$file" ]] || { echo "[ISO][ERROR] Limine missing $file; run build.sh" >&2; exit 5; }
done
STAGE="$(mktemp -d "$ROOT/build/iso_root.XXXXXX")"
trap 'rm -rf -- "$STAGE"' EXIT
mkdir -p "$STAGE/boot/limine" "$STAGE/EFI/BOOT"
cp "$ROOT/build/brahma.elf" "$STAGE/boot/brahma.elf"
cp "$ROOT/boot/limine.conf" "$STAGE/boot/limine/limine.conf"
if [[ -n "$RESOLUTION" ]]; then
    printf '    resolution: %sx32\n' "$RESOLUTION" >> "$STAGE/boot/limine/limine.conf"
fi
cp "$LIMINE_DIR/limine-bios.sys" "$LIMINE_DIR/limine-bios-cd.bin" \
   "$LIMINE_DIR/limine-uefi-cd.bin" "$STAGE/boot/limine/"
cp "$LIMINE_DIR/BOOTX64.EFI" "$STAGE/EFI/BOOT/BOOTX64.EFI"
mkdir -p "$(dirname "$ISO")"
xorriso -as mkisofs -R -r -J \
    -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 \
    -boot-info-table -hfsplus -apm-block-size 2048 \
    --efi-boot boot/limine/limine-uefi-cd.bin \
    -efi-boot-part --efi-boot-image --protective-msdos-label \
    -o "$ISO" "$STAGE"
"$LIMINE_DIR/limine" bios-install "$ISO"
