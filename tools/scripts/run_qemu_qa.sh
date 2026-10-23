#!/usr/bin/env bash
# Secondary UEFI boot gate for Linux/CI (QEMU + OVMF). Windows VirtualBox remains primary.
#
# Usage: run_qemu_qa.sh [--runs N] [--resolution WxH]... [ISO]
#   --runs N          cold boots per configuration (default 1; M1 gate uses 10)
#   --resolution WxH  also boot an ISO variant requesting this Limine mode;
#                     repeatable. Fails if the kernel reports a different mode.
# With no --resolution, boots ISO (default build/parama.iso) at the firmware's
# default mode. Each boot is a fresh QEMU process (cold power-on) and must show
# ordered COM1 markers, no fatal, and a screendump identical to a host render of
# the same kernel display code.
#
# Evidence: build/qa/qemu/<config>/run-NN.com1.log and run-NN.png, plus
# build/qa/qemu/summary.txt. build/qemu-com1.log is the last run's log (CI artifact).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUNS=1
RESOLUTIONS=()
ISO="$ROOT/build/parama.iso"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --runs) RUNS="${2:?--runs needs a value}"; shift 2 ;;
        --resolution) RESOLUTIONS+=("${2:?--resolution needs a value}"); shift 2 ;;
        -h|--help) sed -n '2,15p' "${BASH_SOURCE[0]}"; exit 0 ;;
        -*) echo "[QEMU][ERROR] Unknown option: $1" >&2; exit 2 ;;
        *) ISO="$1"; shift ;;
    esac
done
[[ "$RUNS" =~ ^[1-9][0-9]*$ ]] || { echo "[QEMU][ERROR] --runs must be a positive integer" >&2; exit 2; }
for bin in qemu-system-x86_64 python3 cc; do
    command -v "$bin" >/dev/null || { echo "[QEMU][ERROR] Missing prerequisite: $bin" >&2; exit 2; }
done
[[ -f "$ISO" ]] || { echo "[QEMU][ERROR] Missing ISO: $ISO" >&2; exit 2; }
firmware=''
for path in \
    /usr/share/OVMF/OVMF_CODE_4M.fd \
    /usr/share/OVMF/OVMF_CODE.fd \
    /usr/share/edk2/ovmf/OVMF_CODE.fd \
    /usr/share/edk2/x64/OVMF_CODE.fd; do
    if [[ -f "$path" ]]; then firmware="$path"; break; fi
done
[[ -n "$firmware" ]] || { echo '[QEMU][ERROR] No OVMF UEFI firmware found' >&2; exit 2; }

QA="$ROOT/build/qa/qemu"
rm -rf -- "$QA"
mkdir -p "$QA" "$ROOT/build/tests"
RENDERER="$ROOT/build/tests/render_splash"
cc -std=c2x -O1 -Wall -Wextra -Werror -I"$ROOT/brahma/include" -I"$ROOT/third_party" \
    "$ROOT/tools/tests/render_splash.c" "$ROOT/brahma/core/display/boot_display.c" -o "$RENDERER"

# Configuration list: "label|iso|expected-resolution"
CONFIGS=("default|$ISO|")
for res in "${RESOLUTIONS[@]}"; do
    variant="$QA/parama-$res.iso"
    bash "$ROOT/tools/scripts/make_iso.sh" "$variant" "$res" >/dev/null 2>"$QA/make_iso-$res.err" || {
        cat "$QA/make_iso-$res.err" >&2; echo "[QEMU][ERROR] Could not build $res ISO variant" >&2; exit 2; }
    CONFIGS+=("$res|$variant|$res")
done

{
    echo "PARAMA QEMU/OVMF boot QA ($(date -u +%Y-%m-%dT%H:%M:%SZ))"
    echo "kernel sha256: $(sha256sum "$ROOT/build/brahma.elf" | cut -d' ' -f1)"
    echo "iso sha256:    $(sha256sum "$ISO" | cut -d' ' -f1)"
    echo "firmware:      $firmware"
    echo "emulator:      $(qemu-system-x86_64 --version | head -1)"
    echo "runs per configuration: $RUNS"
} > "$QA/summary.txt"

failures=0
last_log=''
for config in "${CONFIGS[@]}"; do
    IFS='|' read -r label iso expect <<< "$config"
    mkdir -p "$QA/$label"
    passed=0
    for ((run = 1; run <= RUNS; ++run)); do
        n=$(printf '%02d' "$run")
        log="$QA/$label/run-$n.com1.log"
        if result=$(python3 "$ROOT/tools/scripts/qemu_boot_qa.py" --iso "$iso" --firmware "$firmware" \
                --log "$log" --png "$QA/$label/run-$n.png" --renderer "$RENDERER" \
                --expect-resolution "$expect" 2>&1); then
            passed=$((passed + 1))
        else
            failures=$((failures + 1))
            [[ -f "$log" ]] && { echo "--- COM1 ($label run $n) ---" >&2; cat "$log" >&2; }
        fi
        echo "[$label run $n] $result"
        echo "[$label run $n] $result" >> "$QA/summary.txt"
        last_log="$log"
    done
    echo "[$label] $passed/$RUNS boots passed" | tee -a "$QA/summary.txt"
done
[[ -n "$last_log" && -f "$last_log" ]] && cp "$last_log" "$ROOT/build/qemu-com1.log"
if [[ $failures -ne 0 ]]; then
    echo "[QEMU][FAIL] $failures boot(s) failed; evidence in $QA" >&2
    exit 1
fi
echo "[QEMU][PASS] All UEFI boots passed; evidence in $QA"
