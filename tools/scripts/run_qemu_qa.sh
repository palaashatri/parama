#!/usr/bin/env bash
# Secondary UEFI smoke test for Linux CI. Windows VirtualBox remains primary.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:-$ROOT/build/parama.iso}"
LOG="$ROOT/build/qemu-com1.log"
command -v timeout >/dev/null && command -v qemu-system-x86_64 >/dev/null || {
    echo '[QEMU][ERROR] Install coreutils and qemu-system-x86_64' >&2; exit 2;
}
[[ -f "$ISO" ]] || { echo "[QEMU][ERROR] Missing ISO: $ISO" >&2; exit 2; }
firmware=''
for path in \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/edk2/ovmf/OVMF_CODE.fd; do
    if [[ -f "$path" ]]; then firmware="$path"; break; fi
done
[[ -n "$firmware" ]] || { echo '[QEMU][ERROR] No OVMF UEFI firmware found' >&2; exit 2; }
rm -f -- "$LOG"
mkdir -p "$ROOT/build"
# Timeout 124 is expected: the diagnostic kernel deliberately halts after BOOT_OK.
set +e
timeout --signal=TERM --kill-after=2 35 \
    qemu-system-x86_64 -machine q35 -m 512M -smp 1 -nodefaults \
    -device VGA -display none -serial "file:$LOG" \
    -drive "if=pflash,format=raw,readonly=on,file=$firmware" \
    -cdrom "$ISO" -boot order=d -no-reboot -no-shutdown
exitcode=$?
set -e
if [[ $exitcode != 124 && $exitcode != 0 ]]; then
    echo "[QEMU][FAIL] Emulator exited $exitcode" >&2
    [[ -f "$LOG" ]] && cat "$LOG"
    exit 1
fi
if [[ ! -f "$LOG" ]] || ! grep -Fq '[BRAHMA] BOOT_OK' "$LOG" || \
   ! grep -Fq '[BHUMI] Boot frame smoke test passed' "$LOG" || \
   ! grep -Fq '[BRAHMA] Boot diagnostic framebuffer drawn' "$LOG" || \
   grep -Fq '[BRAHMA][FATAL]' "$LOG"; then
    echo '[QEMU][FAIL] Serial milestone missing or fatal error' >&2
    [[ -f "$LOG" ]] && cat "$LOG"
    exit 1
fi
echo '[QEMU][PASS] UEFI kernel boot & Bhumi smoke test'
