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

An eventual goal is to host a **native [SLOPOS](https://github.com/palaashatri/rust-slopos) shell and shared UI toolkit** on PARAMA after BRAHMA/GARUDA/SURYA deliver actual isolated user applications, graphics/input and capability/IPC services. The future integration must use a deliberate SURYA/SLOPOS single-compositor interface, capability-secured platform adapters and real non-Linux native runtimes. System 7/Platinum Classic and optional Loom Design Bible-inspired themes are aspirations; [Loom](https://github.com/palaashatri/rust-loom) application compatibility would require additional independent Rust/Slint platform work.

**PLANNED, not implemented.** This is not part of the current M0–M10 kernel/desktop acceptance; the existing VirtualBox/QEMU boot and isolation milestones come first. See [AGENTS.md §5.1](AGENTS.md#51-future-slopos-desktop-hosting-goal-separate-gated-integration-track).

## Build on Windows (VirtualBox + WSL2)## Build on Windows (VirtualBox + WSL2)

Recommended: Windows 11, PowerShell 7+, Ubuntu under WSL2, VirtualBox 7.2.
VirtualBox runs as a native **Windows** application. WSL only runs the Linux
cross-compilation and ISO packaging tools. Avoid mounting the ISO through WSL
VirtualBox; use the Windows `VBoxManage.exe` wrapper.

In Ubuntu/WSL2, install dependencies (a supported LLVM Clang/LLD toolchain,
CMake >=3.28, Ninja, curl, tar, xorriso, GCC/make, Python 3):

```bash
sudo apt-get update
sudo apt-get install -y clang lld cmake ninja-build curl xorriso \
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

On a Linux machine with QEMU/OVMF installed, a secondary UEFI boot test is:

```bash
bash tools/scripts/run_qemu_qa.sh
```

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

**Not yet proven:** booting with Limine v12.9.3 on VirtualBox/OVMF or QEMU,
serial COM1 handling in a running VM, and correctness of actual display output.
Do not confuse the compiler/host test evidence with the later VM acceptance gate.

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
  tools/scripts/build.ps1       Windows -> named Ubuntu WSL bridge
  tools/scripts/build-docker.ps1 Windows -> Docker Desktop alternative
  toolchain/Dockerfile           Docker-based Linux toolchain
  tools/scripts/run_qa.ps1      Windows VirtualBox harness
  tools/scripts/run_qemu_qa.sh  Linux OVMF smoke harness
  tools/scripts/check.sh        host-side tests
  tools/tests/                  allocator, framebuffer, ELF tests
  .github/workflows/ci.yml     hosted build + boot regression
```

## License

Project code MIT; `third_party/limine_min.h` is 0BSD, derived from the Limine
protocol project. Third-party binaries, future Doom engine code, and game WADs
retain their **own** licenses. No WAD or Doom source is included in this release.
