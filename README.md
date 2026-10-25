# PARAMA OS

A capability-oriented x86-64 operating-system research project: a freestanding
C23 microkernel (**BRAHMA**) and, ultimately, isolated C++23 services and
applications. It is **not a Linux distribution**.

**Actual implemented state:** initial **M0 / M1 code**, compiled ELF and
host tests. First UEFI/VirtualBox boot has **not been verified by the authoring
environment**. There is no process isolation, interrupt dispatcher, compositor,
filesystem, or Doom executable yet. The initial framebuffer screen is a **kernel
boot diagnostic**, not a desktop environment.

The authoritative architecture, ABI proposals, milestones and completion evidence
are in [AGENTS.md](AGENTS.md); do not create separate competing truth documents.

## Future desktop direction: SLOPOS integration

**[SLOPOS](https://github.com/palaashatri/rust-slopos) is the intended eventual full desktop environment on PARAMA**, with BRAHMA as kernel, GARUDA supervising native apps and SURYA as the sole compositor/windowing service. PARAMA M7 retains a **minimal provisional proof shell** for testing graphics/input/restarts; it is not a rival production desktop. SLOPOS would need versioned capability-checked adapters and a genuine non-Linux user-space runtime before anything can run natively.

Future SLOPOS personalization is **Windows XP-class**, not merely dark/light: clean-room **System 7/Platinum Classic**, **Sinty-inspired Modern**, additional declarative styles, independently selectable Dock/taskbar layout profiles, icon packs, accessibility prefs and coherent first-party app controls. PARAMA inherits SLOPOS's licensed symbolic icon manifest and, later, original brand-icon family. Its design rules come from the [SLOPOS Design Bible](https://github.com/palaashatri/rust-slopos/blob/main/docs/CONTRACT.md#52-slopos-design-bible--design-language-principles-tokens-and-review), and it must not fork another theming engine. [Loom](https://github.com/palaashatri/rust-loom) portability remains a separate Rust/Slint/backend challenge.

**PLANNED, not implemented.** This is not part of the current M0–M10 kernel/desktop acceptance; the existing VirtualBox/QEMU boot and isolation milestones come first. See [AGENTS.md §5.1](AGENTS.md#51-future-slopos-desktop-hosting-goal-separate-gated-integration-track).

## Platform compatibility and evidence policy

PARAMA's goal is native capability-enforced computing, not a Linux or Windows compatibility facade. Driver research may reuse concepts and appropriately licensed upstream implementations, but Linux graphics drivers require real kernel-specific DMA, memory, interrupts, firmware and scheduling support—not a simple recompilation. Wine/Raven-like Windows execution remains a separately gated **future user-space compatibility personality** after genuine PARAMA process, storage, graphics and IPC fundamentals are proven.

Every milestone requires repeatable positive/negative VM tests, exact toolchain and source versions, failure traces, component-level licensing and honest hardware/performance claims. A kernel boot splash, loaded executable or isolated benchmark never proves a full desktop or compatibility layer. See [AGENTS.md §5.2](AGENTS.md#52-driver-portability-windows-compatibility-and-evidence-discipline).

## Future Linux userspace compatibility and original apps

[Vinix](https://vinix-os.org/) suggests evaluating a **bounded Linux ELF/syscall compatibility personality** after PARAMA has independently proven native Ring 3, capability IPC, ELF/runtime, files and process lifecycle. [VitruvianOS](https://v-os.dev/) suggests treating session supervision, filesystem metadata and multimedia as coherent platform services. Both are **research references**, not code imports or present functionality; BRAHMA remains a capability-oriented independent microkernel. See [AGENTS.md §5.2.1–2](AGENTS.md#521-vinixvitruvianos-research-compatibility-personalities-services-and-practical-userspace).

A future SLOPOS desktop on PARAMA may expose only **owner-authored, license-audited apps** through an optional catalog. Tricycle and texture-auto-upscaler are forks and excluded from first-party branding. JTorch and Benchmarks are also excluded from the first-party **desktop application** catalog because they are library/QA tools, not desktop products; their independent development continues. Every Java, Python, Rust UI or ML app needs a **real PARAMA-specific runtime/graphics/ABI and memory proof**; no app or Linux/Wine binary runs there merely because its Linux version does. Small-device installations must avoid eager heavyweight runtimes and model files.

## Future Raspberry Pi Zero 2 W efficiency and ARM64 goal

The **Pi Zero 2 W** (ARM Cortex-A53 ×4 at 1 GHz, **512 MB RAM**) is PARAMA's deliberately constrained future hardware reference. Kernel, IPC, driver and compositor architectures should be frugal by design: bounded memory/queues, event-driven idle behavior, prompt native input, safe cancellation, honest low-memory failure and graceful opt-out of heavyweight graphical/AI features. Native SLOPOS on PARAMA would share its Classic/Modern themes while keeping SURYA as compositor.

**This is NOT a supported PARAMA platform today.** PARAMA targets **x86-64 first**, and the Zero 2 W requires a distinct ARMv8-A kernel/boot/interrupt/MMU/peripheral port, actual board drivers and testing—not an x86-64 image or merely recompilation. QEMU ARM virt tests are preparatory, not board proof. See [AGENTS.md §5.3](AGENTS.md#53-efficiency-first-architecture-and-future-raspberry-pi-zero-2-w--aarch64-port).

## 2027 readiness notes

A microkernel's isolation is only as strong as what sits beneath it. [AGENTS.md §2.7](AGENTS.md#27-hardening-dma-isolation-and-memory-safety-baseline-2027-review) adds planned requirements: CPU protections (SMEP/SMAP/UMIP), a Meltdown and Spectre policy, IOMMU confinement for any user-space driver that can DMA, fuzzing of untrusted-input parsers, and later Secure Boot/TPM research. [AGENTS.md §5.4](AGENTS.md#54-2027-hardware-host-and-toolchain-realities) records 2027 hardware realities: USB/xHCI instead of PS/2, ACPI, virtio and NVMe. It also notes that VirtualBox behaves differently under Hyper-V, and that the LLVM toolchain and container image should be pinned. None of this is implemented yet.

## Build on Windows (VirtualBox + WSL2)

Recommended: Windows 11, PowerShell 7+, Ubuntu under WSL2, VirtualBox 7.2.
VirtualBox runs as a native **Windows** application. WSL only runs the Linux
cross-compilation and ISO packaging tools. Avoid mounting the ISO through WSL
VirtualBox; use the Windows `VBoxManage.exe` wrapper.

In Ubuntu/WSL2, install dependencies (a supported LLVM Clang/LLD toolchain,
CMake >=3.28, Ninja, curl, tar, xorriso, GCC/make, Python 3):

```bash
sudo apt-get update
sudo apt-get install -y clang lld libclang-rt-dev cmake ninja-build curl xorriso \
  build-essential python3
```

At the repository root in **Windows PowerShell 7**:

```powershell
pwsh -File tools/scripts/build.ps1 -Distro Ubuntu
pwsh -File tools/scripts/run_qa.ps1 -TargetMilestone BOOT
```

If your WSL distribution is named `Ubuntu-24.04`, use `-Distro Ubuntu-24.04`.
If **Docker Desktop** is installed but no Ubuntu WSL distribution is available,
use this alternative build command instead (then run the same VM QA command):

```powershell
pwsh -File tools/scripts/build-docker.ps1
```

The build command compiles `build/brahma.elf`, downloads **Limine v12.9.3**
from the official release and verifies its SHA-256 before using it, then creates
`build/parama.iso` (hybrid BIOS/UEFI, tested workflow targets UEFI). Internet is
necessary the first time unless the exact release archive is already cached.

The QA command creates or reuses only a VM named `PARAMA-QA` with a special
ownership marker. An existing unmarked VM with that name is **never modified**.
It stores the serial log and a screenshot in `build/` and powers the VM down.
It validates the serial boot and frame smoke test plus PNG capture; visual
inspection is still necessary, and **no user-level graphics/input is claimed**.

To build directly inside Ubuntu/WSL without the PowerShell wrapper:

```bash
bash tools/scripts/build.sh
bash tools/scripts/check.sh
```

On a Linux machine with QEMU/OVMF installed (`sudo apt-get install -y
qemu-system-x86 ovmf`), a secondary UEFI boot test is:

```bash
bash tools/scripts/run_qemu_qa.sh                       # one boot, firmware default mode
bash tools/scripts/run_qemu_qa.sh --runs 10 \
  --resolution 800x600 --resolution 640x480             # M1 QEMU gate, as CI runs it
```

Each boot is a fresh QEMU process. It must show the COM1 markers in order with
no fatal, report a framebuffer mode equal to any `--resolution` requested
(Limine silently falls back when a mode is unavailable, so a mismatch fails),
and produce a screen identical pixel-for-pixel to a host render of the same
display code. Evidence (`run-NN.com1.log`, `run-NN.png`, `summary.txt`) is kept
under `build/qa/qemu/`. OVMF offers no mode below 640x480, so `320x200` cannot
be exercised under QEMU.

## Existing proof

The initial code has been compiled using Clang 17, CMake 3.31 and LLD 17.
Two independent build directories produced bit-identical kernel binaries,
providing a first local build-reproducibility check. The generated kernel is a
freestanding ELF64 `ET_EXEC` for x86-64 with four W^X program
segments, a high-half entry point and Limine markers. Host-side tests of early
page allocation, framebuffer writes and ELF structure have passed.

The downloadable source bundle includes the verified `build/brahma.elf` for
inspection, but **no prepackaged bootable ISO**: the hybrid ISO is assembled
with the pinned official Limine bootloader using `tools/scripts/build.sh`.

**Proven under QEMU/OVMF (2026-10-24):** the pinned Limine v12.9.3 download
and SHA-256 verify; the ISO builds on a fresh Ubuntu 24.04 (Clang 18, LLD 18);
30/30 cold UEFI boots (10 each at 1280x800, 800x600, 640x480) pass the M1
checks, including pixel-exact screen comparison. See AGENTS.md section 0.

**Not yet proven:** booting on VirtualBox (the primary M1 platform) and the
320x200 mode on real firmware. Do not confuse QEMU evidence with the
VirtualBox acceptance gate.

## Project organization

```text
PARAMA/
  AGENTS.md                     single engineering source of truth
  CMakeLists.txt                Clang freestanding kernel build
  toolchain/x86_64-none.cmake   Clang cross-target
  brahma/                       implemented kernel entry + diagnostics + boot memory
  boot/limine.conf              boot loader entry
  third_party/limine_min.h      initial minimal protocol ABI, 0BSD
  tools/scripts/build.sh        reproducible ISO build
  tools/scripts/make_iso.sh     ISO packaging (optional Limine resolution variant)
  tools/scripts/build.ps1       Windows -> named Ubuntu WSL bridge
  tools/scripts/build-docker.ps1 Windows -> Docker Desktop alternative
  toolchain/Dockerfile           Docker-based Linux toolchain
  tools/scripts/run_qa.ps1      Windows VirtualBox harness
  tools/scripts/run_qemu_qa.sh  Linux OVMF boot gate (repeat runs, resolutions)
  tools/scripts/qemu_boot_qa.py one QEMU boot: COM1, screendump, pixel check
  tools/scripts/check.sh        host-side tests
  tools/tests/                  allocator, framebuffer, ELF tests, splash reference renderer
  .github/workflows/ci.yml     hosted build + boot regression
```

## License

Project code MIT; `third_party/limine_min.h` is 0BSD, derived from the Limine
protocol project. Third-party binaries, future Doom engine code, and game WADs
retain their **own** licenses. No WAD or Doom source is included in this release.
