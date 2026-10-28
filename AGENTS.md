# PARAMA OS — Authoritative Engineering Specification / AGENTS.md

**Status date:** 2026-10-24  
**Repository:** standalone freestanding operating-system research project  
**Target:** x86-64, UEFI, Limine, 4-level paging initially  
**Kernel:** BRAHMA, C23, supervisor/ring 0  
**Services:** freestanding C++23, eventually unprivileged/ring 3  
**Primary dev host:** Windows 11 + WSL2 Ubuntu or Docker Desktop for compilation + VirtualBox native Windows QA  
**Alternative CI host:** Linux + OVMF/QEMU  
**License:** MIT for owned code, per-component license compliance for dependencies

> **Truth policy:** This document is the sole source of architectural plans,
> milestone statuses, completion criteria, and multi-agent instructions. README
> may describe how to build and run but must never contradict this file. The
> initial C23 kernel and boot allocator are *not* a complete microkernel.

---

## 0. Executable state and evidence (update with each merged milestone)

Legend: `PASS` means actually exercised and evidence retained; `CODE` means
implemented and host checks pass but hardware acceptance has not been executed;
`PLAN` means not implemented; `FAIL` means observed regression.

| Gate | State | Verified evidence | Still required |
|---|---|---|---|
| M0 source/toolchain | PASS | 2026-10-24, fresh Ubuntu 24.04 (cloud container, not WSL), Clang/LLD 18.1.3: `build.sh` downloads Limine v12.9.3, SHA-256 verifies, ISO built with checksum printed; `check.sh` passes; three clean build dirs give identical kernel SHA-256 `5d38aa69…456ee` | Re-run on a WSL2 host to confirm the Windows path (no change expected) |
| M1 boot diagnostic — QEMU/OVMF | PASS | 2026-10-24, QEMU 8.2.2 + OVMF `OVMF_CODE_4M.fd`, kernel `5d38aa69…456ee`: `run_qemu_qa.sh --runs 10 --resolution 800x600 --resolution 640x480` → 30/30 cold boots; each has ordered COM1 markers, no fatal, granted mode equal to requested, PNG captured, and screen pixel-identical to a host render of `boot_display.c`; PNGs at 1280x800 and 640x480 manually reviewed (text reads correctly). CI runs the same gate and uploads `build/qa/qemu/` | — (320x200: OVMF offers no mode below 640x480; covered only by host unit test) |
| M1 boot diagnostic — VirtualBox 7.2 (primary) | CODE | Same kernel; harness `run_qa.ps1` unchanged | `run_qa.ps1 -TargetMilestone BOOT` on Windows; repetition option in `run_qa.ps1`; 320x200 if the VirtualBox GOP offers it |
| M2 memory and exceptions | PLAN | Early monotonic frame source exists, but no fully managed PMM/IDT | fault tests, allocator reclaim, VMM and paging |
| M3 kernel threading and Ring 3 | PLAN | none | context switch, privilege boundary, isolated tasks |
| M4 capabilities and IPC | PLAN | interface proposals only | security test matrix, transfer/revoke semantics |
| M5 ELF user loader and runtime | PLAN | none | independent ELF launched in ring 3 |
| M6 user-space device services | PLAN | none | framebuffer/input driver task isolation |
| M7 init + desktop | PLAN | none | GARUDA supervision, SURYA and interactive UI |
| M8 asset store | PLAN | none | read-only CONTENT service from application |
| M9 YAMA Doom | PLAN | none | gameplay with input, WAD, frame changes |
| M10 crash/recovery | PLAN | none | fault containment and automated recovery |

**Strict ban on status inflation.** A BIOS/UEFI splash is not a desktop;
painting directly from kernel is not a user-space graphics driver; running Doom
linked into BRAHMA is not a successful Doom user-process milestone. Logs alone
never prove graphics, user input, security isolation, or crash recovery.

### 0.1 What M1 currently does

`brahma/main.c` contains the actual Limine higher-half kernel entry, required
boot-data checks, COM1 115200 8N1 serial messages, LIMINE usable memory-map
inspection, a monotonic 4 KiB frame allocation smoke test using HHDM, a direct
framebuffer diagnostic splash, and a deliberate `cli; hlt` end state. A
read-only bootloader-provided memory map is not a persistent allocator state;
only `LIMINE_MEMMAP_USABLE` regions may be allocated. No interrupts are enabled
in M1, no Ring 3 process exists, and no bootloader-reclaimable memory is freed.

Accepted M1 serial messages: `[BHUMI] Boot frame smoke test passed`, followed by
`[BRAHMA] Boot diagnostic framebuffer drawn` and `[BRAHMA] BOOT_OK`, and no `[BRAHMA][FATAL]`. A valid VM test also preserves a
PNG. `run_qa.ps1 -TargetMilestone BOOT` checks only these narrow properties.
The kernel also prints `[BRAHMA] Framebuffer: WxHxBPP pitch P` before drawing,
because Limine silently falls back when a configured `resolution:` is
unavailable; a resolution gate must compare against this line, not the request.
`run_qemu_qa.sh` additionally requires the markers in order and a screen that
matches a host render of `boot_display.c` (`tools/tests/render_splash.c`)
pixel-for-pixel.

**M1 is signed off on QEMU/OVMF only; the VirtualBox gate is still pending.**
The ISO packaging (`build.sh` → `make_iso.sh`) downloads Limine v12.9.3 only
from the official release, validates SHA-256
`9f42fe9ea2e84d71056529969b1d5c24ba9a2287ffcc01bcce58b0ef8aa98b1d`,
and follows upstream's hybrid ISO method; this was first executed on 2026-10-24.

First execution found two defects that every host check and serial marker had
passed: the glyph table had no `N` (screen read `KER EL O LI E`), and at 320x200
the title clipped at the right edge. Both are fixed, with unit tests.

---

## 1. Non-negotiable system architecture

### 1.1 Goal

PARAMA must ultimately support genuinely isolated user applications and
user-space services on its **own** kernel; it must not use a Linux kernel,
Linux host compositor, or compatibility shell disguised as a native OS.
Limine is only an external bootloader. Initial GUI support may be slow 2D
software rendering and PS/2 input; acceleration and general hardware support
come later and must not block basic correct isolation.

### 1.2 Layering

```
UEFI firmware -> Limine v12.9.3 -> BRAHMA (ring 0)
                                       |
      +--------------------------------+-----------------------------+
      | memory VM / interrupts / syscall ABI / scheduling / caps / IPC|
      +--------------------------------+-----------------------------+
                                       |
           /---------------------------+----------------------\
          /                            |                       \
   GARUDA supervisor           VISHWAKARMA services       CHITRAGUPTA
     (ring 3)                 (video/input; ring 3)     (assets; ring 3)
          |                            |                       |
          +----------> SURYA compositor <---------- YAMA/JIVA apps
                     (ring 3)                       (ring 3)
```

A driver may access only explicitly mapped hardware resources and notification
channels created by BRAHMA. The kernel owns raw IRQ plumbing, I/O permission
policy, page tables, capability tables and process-address-space transitions.
Only GARUDA receives the initial capability graph delegated by BRAHMA. Kernel
must not hide service-specific behaviors in permanent ring 0 code.

### 1.3 Names and bounded responsibilities

| Name | Ownership | Allowed responsibilities | Explicitly not responsible for |
|---|---|---|---|
| PARAMA | project | release, cross-toolchain, policy | pretending to be a runnable subsystem |
| BRAHMA | kernel | trap entry, protected address spaces, minimal object kernel | UI, file semantics, Doom engine |
| KĀLA | BRAHMA core | thread scheduling, timers, sleep/wakeup, CPU-local queues | application event loops |
| BHŪMI | BRAHMA core | PMM, kernel VMM, mapping checks, per-task address spaces | user heap policy |
| DHARMA | BRAHMA core | typed objects, rights, handle tables, revoke/transfer | global service name discovery |
| VĀYU | kernel/lib | IPC endpoints, blocking notifications, shared-memory mapping | pretending a ring buffer alone provides isolation |
| VAJRA | BRAHMA x86-64 | GDT/TSS, IDT, exception handling, IRQ dispatch, syscall entry | C++ service callbacks in ring 0 |
| VISHWAKARMA | service | hardware/resource-specific user-space drivers | arbitrary physical map access |
| GARUDA | service | initial process supervision, restarts, capability handoff | direct kernel-object modification |
| CHITRAGUPTA | service | immutable asset store, optional later persistence | unmediated kernel storage parsing |
| KAVACHA | service/lib | app policy, manifest and isolated launch requests | substitute for kernel enforcement |
| SURYA | service | window surfaces, composition, focus, event routing | direct unmanaged PCI or port I/O |
| YAMA | app | doomgeneric port and game event loop | ring 0 or linker-integrated rendering |
| JĪVA | execution concept | user process/task entity | a separate daemon without code |

---

## 2. Target ABIs, security boundaries and interface proposals

**All designs in this section are _proposed interfaces_, not existing code.**
Before M3/M4 implementation, freeze a `PARAMA_ABI_VERSION=1` shared public
header in `include/parama/abi/`, write both positive and negative tests, then
make all services consume it. ABI changes after freeze require versioning and
compatibility tests; never silently rename existing opcodes.

### 2.1 Process and thread design

- Each user process has a distinct CR3 root, user virtual address space,
  non-executable stack, bounded initial capability table and at least one
  Ring 3 thread. No user mappings may contain kernel secrets.
- x86-64 initial mode: single-core, 4-level page tables; no dependence on
  identity mapping. SMP comes only after uniprocessor is proven correct.
- VAJRA owns GDT, TSS with valid `RSP0`, IDT with fault handlers and IRQ vectors,
  `syscall` entry (or documented `int 0x80` for initial bootstrap). Never use
  `sysret` without canonical-address handling; fall back to a safe `iretq` path.
- A kernel trap frame records callee-saved register state, general registers,
  CS/SS/RFLAGS/RIP/RSP and error code where applicable. x86 exceptions that
  push an error code must not be decoded like exceptions that do not.
- KĀLA initially uses preemptive round-robin with a bounded tick, later
  priority/wakeup-based work stealing; tickless mode is an optimization, not
  an M2 prerequisite. Prove blocking does not spin indefinitely.
- Kernel memory management must check alignments, overflow, user/kernel
  boundaries and execute/write protections. Do not enable interrupts until
  VAJRA has a working IDT and safe kernel stacks.

### 2.2 DHARMA capability model

- Capabilities are unforgeable **within the threat model** by keeping object
  pointers and metadata inside protected kernel memory. The API exposes
  per-process opaque 64-bit handles, proposed encoding `(generation << 32) |
  slot_index` (neither field is a global object identifier).
- Every table slot has `{object, type, rights, generation, parent/revoke link}`.
  Object lifetime uses refcount/ownership, and stale generation cannot resolve.
- Permissions must be explicit and minimal: e.g. `READ`, `WRITE`, `MAP`,
  `SEND`, `RECEIVE`, `GRANT`, `CONTROL`, `IRQ_WAIT`, `PORT_IO`. Arbitrary
  combinations are invalid for incompatible object types.
- The kernel validates rights on each syscall; client libraries cannot bypass
  checks. `TRANSFER` creates a descendant capability with a subset of rights.
  Explicit revoke removes descendants before reuse; no ambient root handle.
- Provisioning physical MMIO/PIO or IRQ authority must require a privileged
  boot capability; never allow users to guess physical addresses and map them.
- Document in tests: forged handle, wrong type, stale handle, grant escalation,
  double revoke, transfer race, process crash during transfer, cross-process
  resource exhaustion and refcount underflow.

### 2.3 VĀYU IPC

- **First implementation:** bounded kernel-mediated synchronous messages with
  explicit endpoint capabilities, payload cap and timeouts. Do not attempt
  zero-copy until M4 core IPC semantics pass.
- Proposed IPC header (little endian on x86-64):
  `version:u16, opcode:u16, flags:u32, request_id:u64, payload_bytes:u32,
  handle_count:u16, reserved:u16`. Keep fixed 24-byte header, 8-byte alignment.
- Max inline payload initially 4 KiB; handle transfers bounded to 8 per
  message. Kernel validates all buffers and capabilities before commit and
  provides all-or-nothing transfer, never partial-capability leaks.
- Messages are transport envelopes; services must version and validate their
  payload schemas and reject unexpected opcodes/lengths.
- Large data (framebuffer, asset data) uses separately allocated shareable
  VM objects. Map only through explicit capabilities with least privilege.
  IPC notification carries buffer handle, frame sequence and damage region.
  Do not assume "zero-copy" means CPU/cache coherence is automatic.
- Ring-buffer fast path later needs producer/consumer indices, memory ordering,
  overflow handling, wraparound, reclamation and a kernel wake mechanism.

### 2.4 Proposed syscall ABI v1 (not implemented)

x86-64 `syscall` proposal: `RAX` opcode, `RDI, RSI, RDX, R10, R8, R9` args,
return `RAX` signed status/value (negative error). `RCX` and `R11` clobbered;
all other clobbers explicitly documented. Kernel must validate canonical user
pointers, lengths, permissions and page fault recoverability. Never directly
trust a pointer from Ring 3, including a pointer inside a supposedly mapped
shared buffer. The tentative syscall set for architectural planning:

| ID | Operation | Essential checks |
|---|---|---|
| 0 | ABI_QUERY | supported version/capabilities |
| 1 | TASK_EXIT | release handles, wakes supervisor |
| 2 | THREAD_YIELD | scheduler ready queue |
| 3 | THREAD_SLEEP_UNTIL | monotonic deadline, overflow |
| 4 | CAP_CLONE_RESTRICT | rights attenuation, generation |
| 5 | CAP_REVOKE | descendants, races, refcounts |
| 6 | ENDPOINT_CREATE | quota, initial rights |
| 7 | IPC_SEND | bounds, receiver, rights, timeout |
| 8 | IPC_RECV | bounds, message schema neutrality |
| 9 | VM_OBJECT_CREATE | page quota, zero-init |
| 10 | VM_MAP | alignment, W^X, user permissions |
| 11 | VM_UNMAP | owner/rights, TLB invalidation |
| 12 | TASK_SPAWN | image provenance, cap manifest |
| 13 | IRQ_WAIT | IRQ capability, acknowledgement |

Opaque capability rights must be enforced regardless of application manifest.
IO/MMIO access is never implicit in `TASK_SPAWN`. Boot-time driver/resource
provisioning can use internal kernel calls, not ambient syscalls.

### 2.5 Runtime and ELF loader

- User-space ELF64 ET_EXEC initially; ET_DYN/ASLR later. Loader validates
  ELF magic/machine/segment offsets, integer overflow, file bounds,
  W^X, entrypoint executable mapping and stack guard pages.
- Accept only explicitly supported relocation classes; do not `memcpy()` an
  untrusted ELF into kernel mappings. Load from verified Limine modules initially;
  CHITRAGUPTA evolves into a service after usable user-space IPC exists.
- `libparama` in freestanding C supplies syscall stubs, `memcpy/memset`,
  basic allocator, monotonic time and bounded IPC. A tiny C++23 runtime adds
  placement new, `new/delete` where supported, constructor initialization,
  panic and `UniqueCap<T>` RAII; no exceptions/RTTI until intentionally added.
- Avoid claiming that doomgeneric runs with only five hooks: it also depends
  on engine support, memory and file I/O. The five DG_* callbacks are a
  **porting API**, not the entire libc/OS contract.

### 2.6 Memory and graphics ownership

- Limine framebuffer is initially used *only* by BRAHMA for early diagnostics.
- VISHWAKARMA later obtains a framebuffer/MMIO capability from GARUDA and
  serves a surface-present protocol; SURYA owns window layout and composition.
  YAMA only writes to its granted surface and sends damage notifications.
- Pixel ABI must specify width/height/pitch, format (ARGB8888 initially),
  coordinate origin, 64-bit overflow checks and explicit frame sequencing.
- Display protocol becomes shared only after M6 device service and M7 SURYA
  exist. Tests must check no out-of-bounds writes, window stacking, input focus,
  pointer clipping, event queue overflow and rendering while a client crashes.
- USB, GPU acceleration and multi-monitor support are post-M10 milestones.

### 2.7 Hardening, DMA isolation and memory-safety baseline (2027 review)

**Proposed requirements, recorded 2026-10-25. Each applies at the milestone named, and none of them is implemented yet.** They exist because a microkernel's isolation claims are only as strong as the CPU features, DMA confinement and parser safety beneath them.

- **CPU protection features (M2/M3):** enable NX, CR0.WP, SMEP, SMAP and UMIP when the CPU reports them. With SMAP on, user-memory access happens only inside explicit copy-in/copy-out helpers that use `stac`/`clac`. Log at boot which features are active. A missing feature is recorded, never assumed.
- **Speculative-execution side channels (before any isolation claim on real hardware):** the higher-half kernel is mapped in every user address space (§2.1), so on CPUs affected by Meltdown, user code could read kernel memory. Read `IA32_ARCH_CAPABILITIES`. If the CPU lacks `RDCL_NO`, use separate user and kernel page tables (KPTI-style) or label the isolation claim as not holding on that CPU. Choose and document Spectre v1/v2 mitigations (retpoline or eIBRS/IBRS, plus `lfence` and bounds masking on user-controlled indices) before M10.
- **Control-flow protection (post-M5 goal):** evaluate Intel CET, meaning kernel IBT through `-fcf-protection=branch` and `ENDBR`, and user-mode shadow stacks once the context-switch path saves CET state. Until then, record the absence.
- **Compiler hardening:** the kernel currently builds with `-fno-stack-protector`, which is acceptable for M1. Once BRAHMA has a per-CPU canary source (M3), switch to a stack protector, and evaluate `-ftrivial-auto-var-init=zero`. Ring-3 runtimes get the stack protector from their first release.
- **DMA isolation (required before any DMA-capable user-space driver):** a ring-3 driver that can program a DMA-capable device (virtio, NVMe, xHCI, network, GPU) can read and write any physical memory unless an IOMMU confines the device. Such a driver therefore needs its own IOMMU domain (Intel VT-d or AMD-Vi, discovered through the ACPI DMAR/IVRS tables), mapping only buffers it was granted through capabilities. Without a working IOMMU the driver is **trusted**, and the documentation must say so. M6's framebuffer and PS/2 services need no DMA, so this does not block M6. QEMU can emulate an Intel IOMMU (`-device intel-iommu`); VirtualBox's IOMMU emulation must be verified before it is relied on.
- **Memory safety for C23/C++23:** parsers of untrusted input (the ELF loader, IPC payload validation, capability-table operations, future filesystem and asset parsing) get host-side fuzz harnesses (libFuzzer or equivalent) run with AddressSanitizer and UndefinedBehaviorSanitizer, plus negative tests in the existing M2–M5 gates. **Owner decision (2026-10-25): no Rust.** BRAHMA stays C23 and services stay C++23. Memory safety comes from the fuzzing and sanitizer rules here, the compiler hardening above, RAII capability wrappers (`UniqueCap<T>`), bounds-checked views (`std::span` with checked access in services) and the capability isolation itself.
- **Secure and measured boot (post-M10 research):** UEFI Secure Boot through a signed loader chain, and TPM 2.0 measured boot of BRAHMA and its init modules. VirtualBox 7 and QEMU with OVMF/swtpm can emulate both for testing. No trust claim is made until the chain is verified.

---

## 3. Coding, build, quality and repository contracts

### 3.1 Compiler and ABI rules

Kernel: `-std=c2x -ffreestanding -fno-pic -fno-pie -mcmodel=kernel
-fno-stack-protector -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -mno-80387
-Wall -Wextra -Werror`; preserve symbols and avoid linking host CRT/libc.
The linker script uses a higher-half ELF with separate R/W, R/X, R segments.
No hosted malloc, printf, exceptions, TLS or hidden GNU extensions in BRAHMA.

User-space services, **when implemented**, use C++23 freestanding compilation:
`-std=c++23 -ffreestanding -fno-exceptions -fno-rtti`, with explicit runtime
support. Pointer ownership should use RAII wrappers; kernel must remain
cautious with compiler-emitted runtime helpers.

Use ASCII filesystem paths (e.g., `bhumi`, `kala`, `surya`, `parama.iso`) even
when architectural terminology contains Sanskrit diacritics. This avoids
avoidable Windows shell/path portability failures.

### 3.2 Folder ownership

```text
PARAMA/
  AGENTS.md                        authoritative docs and state
  README.md                        installation/build quickstart
  CMakeLists.txt                   top-level kernel target
  boot/                            Limine config, future init modules
  toolchain/                       deterministic architecture toolchain
  third_party/                     vendored interfaces with license notices
  brahma/
    arch/x86_64/                  Limine handoff, serial, IDT, trap/syscalls
    core/bhumi/                   PMM/VMM (boot allocator currently)
    core/kala/                    planned scheduler
    core/dharma/                  planned capability management
    core/vayu/                    planned kernel IPC
    core/vajra/                   planned protected trap support
    core/display/                 M1 diagnostic framebuffer only
    include/parama/               kernel-owned public internal headers
  include/parama/abi/              planned frozen cross-process ABI
  lib/libparama/                   planned Ring 3 runtime library
  services/garuda/                 planned init/supervisor
  services/vishwakarma/            planned user-space hardware services
  services/chitragupta/            planned asset store
  services/surya/                  planned compositor
  services/kavacha/                planned launcher/policy
  apps/yama/                       planned Doom port
  tools/tests/                     host unit tests and ELF checks
  tools/scripts/                   ISO build, host unit tests, VM QA
  .github/workflows/              build + emulator CI
```

Directories marked `planned` are deliberately not fake-populated in M1. Add
only when an independently buildable target and corresponding test exist.

### 3.3 Windows build policy

- Windows host controls **native** VirtualBox via `VBoxManage.exe`.
- WSL2/Ubuntu or the provided Docker Desktop toolchain performs compilation
  and ISO generation via CMake/Ninja/Clang/LLD and xorriso. Don't attempt to run a Linux VBox binary inside WSL to
  control the host hypervisor. Keep filesystem writes limited to the repo's
  `build/` and the dedicated PARAMA-QA VM configuration.
- The ISO tool pins and checksums the upstream Limine binary asset and does
  not require writing bootloader assembly or building Limine from unknown HEAD. Docker base image/package
  versions are not yet digest/apt-pinned, so strict cross-host toolchain
  reproducibility is a later gate.
- CI Linux QEMU/OVMF supplements but does not replace native VirtualBox tests.
- Never delete, alter or take over a preexisting unmarked VirtualBox machine;
  never install a boot sector on a physical host disk or change host firmware.
- Preserve `build/com1.log`, `build/qa_screen.png`, core/trap diagnostics and
  a failed-run reproduction command. Keep serial messages deterministic.

### 3.4 Agent review checklist for every PR

1. Declare milestone and subsystem ownership; include a one-paragraph
   behavioral contract and failure cases before coding.
2. Implement the smallest vertical slice that proves that contract. No placeholder
   success logs, fake screenshots, unconnected declarations or dead stubs.
3. Run the cross-target kernel build with warnings treated as errors; no
   accidental libc or host linker startup. Run `bash tools/scripts/check.sh`.
4. For boot/runtime changes, run `pwsh .../run_qa.ps1 -TargetMilestone BOOT`
   on Windows VirtualBox (or QEMU/OVMF CI with explicit environment labeling).
5. Inspect full COM1, screenshot where applicable, kernel faults. A COM1 text
   milestone is NOT equivalent to graphics or event-input verification.
6. Add negative tests: invalid pointers, malformed IPC, deliberately thrown
   #PF/#GP, OOM, concurrent handle transfer, service crash as relevant.
7. Update the status table in Section 0 with *actual executed* tests and
   evidence references. Report unverified steps explicitly.
8. No merge with failing checks; no success labels for simulation-only tests.

### 3.5 Agent concurrency rules

Parallel agents work on atomic files/directories with one integration owner.
No independent agent may simultaneously edit global linker/CMake/AGENTS ABI
without the integration owner's approval. Assign work packages such as:

| Agent | Ownership slice | Required output |
|---|---|---|
| A: Build/CI | `tools/`, `.github/`, `toolchain/` | reproducible build, fixed SHA, VM gates |
| B: BHŪMI | `brahma/core/bhumi/` | alloc/free, guards, self-tests |
| C: VAJRA | `brahma/arch/x86_64/`, `brahma/core/vajra/` | trap frame, exception unit/integration tests |
| D: ABI | `include/parama/abi/` | layout static asserts, error and rights model |
| E: KĀLA | `brahma/core/kala/` | scheduler queue tests after trap contract frozen |
| F: Runtime | `lib/libparama/` | user startup and clean ELF once ABI frozen |
| G: User services | `services/` | separate targets, only after runtime/IPC gates |
| H: Desktop/YAMA | `apps/`, `services/surya/` | only after graph/asset/input protocols frozen |
| Integrator | `AGENTS.md`, root build and linker changes | review, merge, end-to-end VM proof |

Do not split across permanent git branches as a substitute for component
ownership. Use short-lived PRs/branches and integrate to main. If a contract
is unresolved, write the tests and request a review; do not have multiple
agents invent incompatible headers.

---

## 4. Milestones and acceptance gates

### M0 — Reproducible kernel build (first prerequisite)

**Implementation:** Clang C23 freestanding ELF, higher-half linker script,
Limine request table, pinned Limine v12.9.3 artifact in ISO packaging.

**Gate:** host compile `-Werror`, ELF checker verifies ELF64/x86-64, four
non-overlapping W^X load segments, valid entry and request markers. ISO creation
must pass in a fresh WSL Ubuntu environment; SHA mismatch aborts. No unpinned
network downloads. Store `build/parama.iso` checksum in build output.

### M1 — BRAHMA first light

**Implementation:** serial 115200 8N1, base-revision check, `HHDM` and
memory-map requests, early boot frame smoke test, initial framebuffer splash.

**Gate:** under VirtualBox 7.2 UEFI, `COM1` has ordered markers plus no fatal,
PNG screenshot shows **PARAMA OS / BRAHMA BOOT / KERNEL ONLINE**, no unexpected
exceptions, deterministic repeated power-on/power-off 10 times. Verify at
320x200 and at one resolution >=800x600. Capture PNG and COM1 for each run.
`run_qemu_qa.sh --runs N --resolution WxH` provides controlled repetition and
mode selection for QEMU/OVMF; `run_qa.ps1` still runs one boot and needs the
same option before ten-run VirtualBox stability can be declared. OVMF offers no
mode below 640x480, and UEFI GOP firmware generally may not offer 320x200 either;
a mode the firmware cannot provide is recorded as unavailable, never as passed.

### M2 — BHŪMI and VAJRA

- Physical frame manager from normalized Limine map; frame allocate/free;
  no double free, never allocate non-usable/reserved types; zero all newly
  exposed memory; bitmaps/buddy or free lists with defined ownership.
- Kernel page tables independent from bootloader; temporary HHDM preserved,
  W^X, no user mappings of kernel; map/unmap, TLB invalidation; guard pages.
- GDT, IDT, TSS, IRQ and exception stubs. Print #PF CR2 + full saved context;
  recover/kill Ring 3 faults later; fatal kernel faults stop with diagnostics.

**Gate:** allocator random stress + exhaustion, hostile malformed memory maps,
allocation/free uniqueness, deliberate exception log verifies vector and
registers, no triple-fault/reboot; no interrupts enabled prematurely. The boot log lists which §2.7 CPU protection features (NX, WP, SMEP, SMAP, UMIP) are active.

### M3 — KĀLA tasking and Ring 3

- Thread struct and CPU state; timer source/calibration and safe preemption.
- Address-space split, TSS `RSP0`, user transition, cooperative yield then
  preemption. Two independently mapped tasks with one task deliberately
  reading the other's private page.

**Gate:** bounded fairness, task isolation #PF without kernel crash, no shared
kernel stack, reliable idle path, 1000 context switches with register checks.

### M4 — DHARMA + VĀYU capability IPC

- Typed per-process capability tables and rights checks, copy/attenuate,
  transfer, revoke and robust object lifetime.
- Bounded kernel endpoints, blocking send/recv, kernel wakeups/timeouts,
  atomic transferred capability delivery, no queue corruption on overflow.

**Gate:** malicious receiver and sender test matrix; stale handle denied,
forged and escalated rights denied, revoked grant inaccessible, data isolation,
service kill while message blocked, no leaks after 10k transfers.

### M5 — GARUDA bootstrap + ELF64 process runtime

- Boot module list and verified read-only init images (Limine modules).
- ELF64 loader and user heap/runtime (`libparama` C and C++23). Initial
  process gets only declared capabilities; no ambient root privilege.
- GARUDA is a **genuine Ring 3 ELF** launched by BRAHMA, not a kernel
  function; it supervises two other independent ELF processes.

**Gate:** ELF fuzz corpus invalid file rejects safely; stdlib minimum tests;
C++ global initialization when claimed; crash one child, GARUDA survives and
restart policy works; serial process PID/CR3 traces are corroborated with
address-space inspection.

### M6 — VISHWAKARMA driver services

- Kernel-brokered video framebuffer mapping and PS/2 input access to isolated
  driver tasks. IRQ notification capabilities and bounded event queues.
- No user-space hardware authority without capability; port permissions
  must be precisely scoped. Display/video protocol v1 defined.
- M6 drivers use no DMA. A later DMA-capable driver is accepted as isolated
  only under §2.7's IOMMU rule; otherwise it is documented as trusted.

**Gate:** simulated pointer/key events originate from VM-injected scancodes,
driver tasks are in ring 3, killing input driver leaves kernel alive, and
invalid driver accesses cause only that driver to fail.

### M7 — SURYA desktop and GARUDA restart

- Window/surface registry, software compositor, cursor, background, focus,
  clipping and damage tracking; basic UI component toolkit starts here.
- A real input-driven desktop shell process with at least two independent
  client surfaces, a focused window, and a stable render loop.

**Gate:** screenshot image-content checks, input test causes a changed pixel
region and changed surface focus, client buffer bounds enforced, repeated
window open/close without leaked capabilities. Killing compositor triggers
supervised clean restart.

### M8 — CHITRAGUPTA immutable asset server

- Hash-verified blob catalog and read-only named lookup protocol, backed by
  mapped initrd initially; no storage device dependence for Doom proof.
- Path/key normalization, size limits, hash agreement, denial of unauthorized
  or missing assets. Use persistent filesystem only in later milestones.

**Gate:** valid blob read, forged/mismatched hash and oversized blob rejected,
asset I/O does not require Ring 0 file parsing, client cannot write server
memory and service can restart.

### M9 — YAMA isolated Doom

- Incorporate a properly licensed doomgeneric revision and document its
  source provenance. Include required runtime/file/memory/timing/input APIs;
  do not pretend DG_* hooks eliminate libc dependencies.
- `DG_Init` opens video, asset and input endpoints; `DG_DrawFrame` publishes a
  sequence-numbered ARGB8888 surface and damage notification; `DG_GetKey`
  reads input events; `DG_GetTicksMs` uses monotonic kernel time; `DG_SleepMs`
  uses scheduler blocking rather than a busy-spin.
- Prefer separately licensed Freedoom WAD for distributable CI and demos.
  Do not commit commercial DOOM1.WAD or unreviewed proprietary assets.

**Gate:** actual independently loaded user Doom executable; menu visible in
screenshot, successive frames advance, keyboard controls change gameplay,
WAD validated by CHITRAGUPTA, disabling Doom capability prevents unauthorized
read, quitting returns to SURYA, kernel remains alive.

### M10 — Production-direction reliability gate (not "production OS" claim)

**Gate:** 100 cold boots, 10k IPC operations under fault injection, repeatable
Doom start/exit, process leak checks, negative rights tests, service restart
and screenshot parity. No privileged escalation in tests. Persistent-storage,
network, USB, sound, accelerated graphics and hardening are **separate future
milestones**; do not call M10 a production-complete general-purpose OS.

---

## 5. Visual quality and future platform direction

After M7, implement a reusable primitive/component library before first-party
apps. SURYA shell should use an explicit theme/layout/component contract
instead of many incompatible hardcoded widgets. Basic first-party app roadmap:
launcher, settings, terminal, file manager, task monitor, notification
center, screenshots, network/audio control (when drivers exist), text editor.
Only expose controls that work against real system services. HDR, VRR,
accelerated compositing, USB, Wi-Fi, hardware compatibility, suspend/resume,
accessibility, localization, software installation and secure updates are
later foundational work, not decorative checkboxes.

Hardware scope is **VirtualBox and QEMU x86-64 first**. Real PCs need firmware,
ACPI/APIC/IOMMU support, PCI enumeration and drivers, plus safety/recovery
strategies. Never encourage installing this unfinished kernel onto a primary
disk, erasing partitions or changing Secure Boot settings for the experiment.

### 5.1 Future SLOPOS desktop-hosting goal (separate, gated integration track)

**Status: DESIGNATED FUTURE DESKTOP / NOT IMPLEMENTED.** [SLOPOS](https://github.com/palaashatri/rust-slopos) is PARAMA's **intended eventual complete desktop environment**, not just one optional shell among competing full desktop projects. PARAMA's M7 SURYA graphical proof still needs its own minimal bootstrap shell to validate independent surfaces, focus, input and restart. That proof shell is **not a separate production DE**. The eventual SLOPOS UI is gated on genuinely isolated user applications, complete enough services and graphics support. This designation does **not** alter BRAHMA, GARUDA, SURYA, the current M0–M10 acceptance, or PARAMA's independence from Linux.

**Non-negotiable boundaries:**

- SLOPOS today targets Linux, Rust, X11/Openbox, GTK-backed components and Linux service providers. These APIs **do not exist on PARAMA**. Future SLOPOS on PARAMA must use an explicit capability-checked, versioned **PlatformBackend** port for process launch, storage/files, clock, threads, IPC, shared memory, window surfaces, input, clipboard, font/text, audio, network and OS permission services. Unsupported services are exposed as unavailable, never faked by Linux shims.
- **One compositor authority and one intended desktop UI:** SURYA owns the graphics/display/input composition lifecycle; SLOPOS owns launcher, Dock, menu bar, shell, native UI toolkit, Settings, apps, themes and user-facing interaction. Preferred port is capability-restricted SLOPOS processes as SURYA clients through versioned surface/input/clipboard protocols. Porting SLOPOS compositor internals *into SURYA* would require a separate explicit architecture review; never run two competing compositors. An eventual Linux Wayland backend is not a PARAMA ABI.
- The SLOPOS toolkit and shell must run outside BRAHMA/ring 0, with the same capability/IPC constraints as any untrusted native app. Keep fonts, layout, rendering and theme data in user mode; no extra kernel GUI code or omnipotent screen/input agent.
- Optional [Loom](https://github.com/palaashatri/rust-loom) integration is a **further independent dependency**: Loom uses Rust/Slint, and a native PARAMA target would require an actually working Rust target/runtime, allocator/threading, filesystem, graphics/Slint backend, input/font services and app packaging. SLOPOS being shown on PARAMA is not proof that Loom runs.
- Future SLOPOS Vision and Intelligence features must obey PARAMA's capability security. Model inference, screen capture, app automation and any cross-app content sharing require revocable, scoped permissions, clear user intent and enforced isolation. X11-style global unrestricted input capture is not a future security contract.
- Support clean-room licensing, original assets, separate build/acceptance evidence and independent projects. Do not let a desired themed demo redefine the kernel, IPC, filesystem, driver or release-readiness gates.
- **Shared customizable presentation:** PARAMA must **not** fork or reimplement a second full theme engine. Future native SLOPOS on PARAMA inherits SLOPOS's Windows XP-class declarative visual styles, independent desktop layout/interaction profiles, icon packs, accessibility rules and user preferences. Built-in styles are clean-room **System 7/Platinum Classic** and **Sinty/Singularity-inspired SLOPOS Modern**, with third-party theme packages and interchangeable Dock/taskbar layout only where the native backend supports them. A theme pack is data: no embedded arbitrary privileged code or silent permission changes. Respect licensing for Sinty's external design reference. The design language, semantic token roles, theme matrix, anti-patterns and review checklist come from the **SLOPOS Design Bible** (`rust-slopos/docs/CONTRACT.md` §5.2). PARAMA's backend consumes it unchanged and does not add PARAMA-specific design rules. Where SURYA or the hardware cannot render a style's effect (for example blur or elevation on the Pi Zero 2 W in §5.3), the backend reports a documented capability fallback; it does not restyle locally.
- **Shared icon identity:** SLOPOS's `assets/icons/symbolic/app-symbols.json` and `assets/icons/symbolic/lucide/` contain a **licensed Lucide symbolic prototype set** (12 app-list glyphs), not original colorful app-brand icons. PARAMA should consume the same versioned manifest/assets through an authorized port, with attribution preserved, instead of making a competing PARAMA icon family. Original colorful Dock/launcher art and graphical/runtime integration remain later milestones.

**Prerequisites before any integration prototype:** M3 genuine ring-3 process isolation; M4 capability transfer/revocation and IPC; M5 real independent user executable loader/runtime; M6 usable user-mode input/video interfaces; M7 real GARUDA/SURYA graphical app lifecycle; adequate filesystem and rendering APIs for the bounded sample. Subsequent daily-use positioning depends on reliable persistent storage, keyboard/mouse/hardware drivers, update/recovery, networking/audio and policy security—these are **not** all delivered by M10.

| Future gate | Proposed evidence (after prerequisites, not a current milestone) |
|---|---|
| PRM-SLOPOS-01 | Freeze a minimal versioned SURYA–SLOPOS surface/input/clipboard ABI and its capability ownership; denied/invalid capability tests must fail closed. |
| PRM-SLOPOS-02 | Boot PARAMA in QEMU/VirtualBox and start a **separate ring-3 SLOPOS UI sample** under GARUDA, with real framebuffer/window, pointer and keyboard, both Classic/Modern theme token states and an authentic symbolic glyph, and verified resource isolation. |
| PRM-SLOPOS-03 | Launch, close, crash and restart the sample independently of BRAHMA/SURYA; prove no cross-process surface/input leakage, unauthorized control or kernel panic; record screenshots and serial/VM artifacts. |
| PRM-SLOPOS-04 | Only when durable file semantics exist, open/save/reopen a native sample; optionally evaluate Loom app portability as its own **separate** future work, never infer it from the sample GUI. |

**Counterparts elsewhere (pointers only; each project evidences its own gate):** PRM-SLOPOS-01 to -04 ↔ SLOPOS FUT-PARAMA; the future additional design gate below ↔ SLOPOS FUT-UI (`rust-slopos/docs/CONTRACT.md` §21.7.6); PRM-SLOPOS-04's Loom note ↔ Loom's `AGENTS.md` §15, which assumes no PARAMA target; §5.3 ARM-04 ↔ SLOPOS EDGE-04 and Loom LOOM-EDGE-04.

These entries are aspirations, not runnable QA claims. Do not rename an M7 framebuffer primitive a completed SLOPOS desktop, and do not schedule this track before independent milestone prerequisites are verified. **Future additional design gate:** independently change a safe visual style, icon pack and desktop layout profile (e.g. Dock versus taskbar), preserve a11y and application behavior, and rollback malformed or unsupported theme data. This is not an M0–M10 gate.

### 5.2 Driver portability, Windows compatibility and evidence discipline

**Engineering rule effective now; compatibility features are FUTURE / NOT IMPLEMENTED.** The Windows-runtime approach explored by projects such as [Raven](https://useraven.org/news/introducing-raven/) is inspiration for separating execution personalities from host services, **not** a native PARAMA port, a Raven dependency, or approval to divert work from M0/M1. The executable evidence, ABI contracts, capability boundaries and M0–M10 gates remain authoritative.

**Reuse tested foundations where sound; don't equate Linux source with portable drivers.**

- Before authoring a kernel subsystem, graphics driver, runtime, parser, OS abstraction or full protocol implementation, compare existing upstream implementations, exact versions, platform assumptions, licenses and long-term maintenance. Define a **minimal versioned abstraction** and an actual failing use case before choosing to rewrite. A reuse decision must survive security, determinism and the C23 kernel/C++23 user-service boundary; third-party reuse does not excuse unverified behavior.
- Linux DRM/KMS/Mesa and Intel/AMD/NVIDIA driver research is relevant, but Linux drivers depend on kernel-specific memory allocators, DMA/IOMMU, interrupts, synchronization, worker queues, power management, GEM/TTM, firmware and driver-model contracts. **Source access does not make them drop-in drivers for BRAHMA.** First verify software framebuffer → VirtIO GPU and SURYA's compositor client protocol in QEMU/VirtualBox; only then design a versioned compatibility or adapter strategy for one selected hardware family, with hardware/firmware-specific proof. Keep user-mode drivers in VISHWAKARMA and hardware rights behind DHARMA/BRAHMA. Evaluate permissive/compatible code and fully document any GPL-derived driver obligation.
- Wine/Proton, Raven or another PE/Windows-API runtime might inform a **future user-space Windows personality**: PE image loading, Windows user-mode APIs, objects/handles, processes/threads, registry/filesystem and graphics translation behind capability-checked PARAMA service adapters. No Windows emulation logic belongs in BRAHMA merely to run applications. On Linux-hosted SLOPOS, Wine/Proton integration is the near-term path; PARAMA must independently implement required process/VM/IPC/runtime/filesystem/graphics facilities, and its security architecture must not be bypassed to satisfy Win32 compatibility.
- Define **separate compatibility gates** for minimal PE loader, basic console GUI, install/uninstall and persistence, window interaction, real graphics API use, and full real-world application workflows. A loaded executable, framebuffer splash or DLL symbol resolving does not prove Windows compatibility, GPU acceleration or game support. Raven's host dependencies must be ported or replaced, never assumed available.
- For any upstream component: retain the exact source commit/hash, documented license for every relevant component, firmware/microcode license and provenance, redistribution obligations, update owner and negative test cases. MIT applies to PARAMA's authored code only; do not label a combined GPL-derived kernel/runtime binary MIT without an explicit reviewed licensing decision.

**Experimental evidence contract, even for early boot milestones:**

| Gate class | Minimum defensible evidence |
|---|---|
| Build | Pinned toolchain/bootloader, reproducible binary hashes, ELF/ABI validation, source SHA and full failure logs |
| Kernel correctness | Fault injection, invalid capability/IPC inputs, memory safety invariants, process isolation, revocation, resource cleanup and controlled service restart when that functionality exists |
| Graphical client | Real independent user process, actual input→surface→frame change in an isolated VM; focus, lifecycle, keyboard, multi-window and permission-denied tests |
| Driver/hardware | Emulator model/firmware, real device IDs, IOMMU/DMA/interrupt behavior, API compatibility, driver version and separate hardware QA; emulator screenshots do **not** establish physical GPU support |
| Windows runtime (future) | Per-runtime and application version: install, launch, UI interaction, persistence, relaunch, rollback/uninstall, negative cases, exact unsupported APIs and truthful per-stage results |
| Benchmark | Exact build/hardware/VM configuration, fixed workload, multiple trials, median/tail latency, boot timing, throughput, resource use, faults/crashes and side-by-side baseline under matched conditions |

- Compare systems only with matching capabilities and workload. Faster launch of a tiny partial implementation cannot demonstrate superiority to Wine or a full Windows runtime. Preserve test failures and explicitly label `NOT_RUN`, `BLOCKED` or `PARTIAL` rather than declaring completion through test stubs or unconditional serial messages.
- AI agents implement bounded approved interfaces and independently tested invariants; do not let a generated code plan redefine kernel architecture. Each PR includes scope/non-goals, reused dependency evaluation, ABI/security/license impact, reproducible commands, artifacts and hashes, negative cases, failure traces and unmet acceptance gates. Integrator retains single-source truth ownership under §§0/3/6. Production/hardware/Windows/SLOPOS promises require actual evidence, not speculative architecture prose.

### 5.2.1 Vinix/VitruvianOS research: compatibility personalities, services and practical userspace

**FUTURE RESEARCH ONLY — NOT IMPLEMENTED; do not reorder M0–M10.** Vinix (https://vinix-os.org/) illustrates independently developed kernels exposing enough Linux syscall/ELF/musl behavior to run selected Alpine binaries; VitruvianOS (https://v-os.dev/) illustrates keeping Linux drivers while layering BeOS APIs through service/kernel bridges. Neither project is PARAMA, their architectures differ (Vinix monolithic versus BRAHMA microkernel; VitruvianOS Linux kernel versus independent BRAHMA) and their performance/compatibility statements are not evidence of PARAMA support. Upstream code has not been imported.

**Preferred long-term ecosystem order:**

1. **Native first:** verified process isolation, memory manager, capability IPC, GARUDA supervision, safe ELF loader, libc/runtime and a functional small SLOPOS shell atop SURYA. Treat basic native Unix-style APIs as a portability aid without weakening DHARMA rights.
2. **Portable command-line applications:** evaluate a small libc compatibility surface, POSIX semantics and useful programs such as BusyBox/SQLite **only where toolchain and license permit**; compare build-from-source portability with any alternative syscall emulation.
3. **Opt-in Linux ABI personality:** user-space-supervised syscall dispatcher and process model emulating a deliberately bounded **x86-64 or AArch64** Linux target with ELF interpreter, TLS, `mmap`, `clone`/`futex`, signals, file descriptors, poll/epoll, and relevant musl/glibc ABI behavior. Some Linux behaviors may require narrowly reviewed BRAHMA primitives, not an unrestricted kernel rewrite. This is **an architectural candidate**, not promise of comprehensive binary compatibility, security-equivalent Linux namespaces, or running Alpine distributions.
4. **Graphical third-party runtimes:** add compatibility display/input/IPC and dynamic-library behaviors only after audited native platform services. The SLOPOS toolkit stays native; Linux UI frameworks and later Wine/Proton are **independently gated** and not automatically portable. Never advertise Windows game support from a trivial ELF/PE loader.

**Native PARAMA and compatibility guest capabilities must remain separate.** A Linux-compatible guest must not receive global input/screen/device/file authority by default; define process/address-space rights and sandbox semantics explicitly. Reject rather than silently disable sandbox requirements; test negative cases. Keep ABI versions, reproducible syscall/ELF conformance fixtures, unsupported syscall list, graceful `ENOSYS`/error semantics, fault isolation and disk/network file lifecycle evidence.

**Vinix resource lesson:** benchmark real boot-to-app memory and responsiveness under low-RAM constraints, not an isolated kernel boot figure. Optional compatibility servers must be unloaded when unused and preserve the future 512 MB Raspberry Pi Zero 2 W target. Platform-specific ARM64 porting is a separately gated hardware program, not enabled by this userspace plan.

**VitruvianOS integration lesson:** copy *boundaries*, not its Linux-specific kernel modules. GARUDA should supervise boot, service activation/restart, seat/display ownership and orderly shutdown; SURYA is the single compositor and eventual SLOPOS is its client. Metadata/FS watches and typed shared-media services belong in capability-governed user-space servers after real filesystem, IPC, event delivery, audio and device driver contracts exist. No BeOS/Haiku API compatibility is implied or newly scheduled. Preserve accurate per-service resource accounting.

| Future research packet | Prerequisite | First proof (NOT_RUN) |
|---|---|---|
| COMPAT-01 | M3–M5 native protected user programs | Small portable libc/CLI executable under native capabilities; full loader/ABI and denied-resource tests |
| COMPAT-02 | COMPAT-01 and audited Linux ABI design | Versioned Linux syscall/ELF/musl subset with actual unmodified tiny guest program, correct error/futex/signal cases, no host privilege escape |
| COMPAT-03 | COMPAT-02, SURYA/input, independent graphics/runtime | Selected actual third-party GUI workflow; native vs guest resource accounting; no unverified Wine/Alpine generality |
| SVC-SESSION | GARUDA/SURYA driver and IPC proof | Crash/kill/restart, input and surface release, focus/seat lifecycle, absent-service behavior in a booted VM |
| SVC-METADATA/MEDIA | Functional file/event/audio services | Real native metadata event and simple media flow with revoked capability negative cases, no eager always-on workloads |

These do **not** replace the existing M0/M1 build/boot mission or SURYA's M7 demonstration, and are not current production gates.

### 5.2.2 Original SLOPOS first-party applications on PARAMA (future portability only)

**Explicit non-candidates:** JTorch is an independent Java numerical library and Benchmarks is an independent JVM QA/benchmark harness. Neither is a bundled SLOPOS/PARAMA desktop application, a Software catalog first-party entry, or a required PARAMA port. Their normal developer-library/tool use on platforms they independently support is unaffected. Do not assign app-icon, shell/theme, GPU/UI port or SLOPOS-specific milestone work to them.

SLOPOS defines the **curated original-project inventory and fork exclusion policy** in `rust-slopos/docs/CONTRACT.md` §21.8.2. PARAMA is a potential additional host for only those independently authored, license-audited apps once it has proven their runtime/toolkit dependencies. Explicitly exclude upstream forks such as `palaashatri/tricycle` and `palaashatri/texture-auto-upscaler` from first-party branding. An upstream library may be reused under its license, but it is not claimed as authored by the SLOPOS/PARAMA team.

Future application targets require **runtime readiness gates**:
- Rust/Iced/Slint: native target ABI, allocator, threading, renderer, events, filesystem, input/clipboard, packaging.
- JVM/Swing/Java libraries: proven supported JDK/JRE, Swing/AWT/headless needs, ONNX or native dependency support and process memory budget; no Java runtime is available by assertion.
- Python/CustomTkinter/Diffusers: supported interpreter, Tk UI, PyTorch/native libraries, graphics, filesystem and clear memory preflight; do not claim 512 MB local diffusion execution.
- Linux/Windows binaries: no native target until actual compatible Linux ABI personality or Windows API layer is independently verified; "SLOPOS target" on Linux must not be reinterpreted as PARAMA binary support.

A lightweight core PARAMA/SLOPOS image must **not** preload all optional runtimes, model weights or developer tools. Preferred distribution is basic native apps + permissioned software catalog; app suites are installed only on supported hardware and with explicit user action. For each eligible app, retain an independently verified portability matrix (native source port / compatibility guest / blocked; platform ABI; UI and model dependencies; minimum memory; functionality and license). Nothing in this section calls these integrations complete.

### 5.3 Efficiency-first architecture and future Raspberry Pi Zero 2 W / AArch64 port

**Reference device:** Raspberry Pi Zero 2 W, **quad-core 1 GHz ARMv8-A Cortex-A53 / 512 MB shared RAM / VideoCore IV** (hardware specification: https://www.raspberrypi.com/documentation/computers/zero.html). **Status: DESIGN GOAL / NOT IMPLEMENTED / NOT IN M0–M10 CURRENT x86-64 ACCEPTANCE.** PARAMA's existing x86-64 BRAHMA/Limine/UEFI/VirtualBox-QEMU milestone plan remains authoritative. Aarch64 ARMv8-A is a **different instruction set and board bring-up**, not an extra compiler flag or something validated by x86-64 VM acceptance.

**Kernel/service efficiency contracts applicable to current code reviews:**

- Keep BRAHMA minimal and predictable: no high-frequency busy-polling or periodic redraw when idle; interrupt/timer-driven waits, bounded IPC queues, bounded handles and object pools, lazy provisioning, zero-copy/shared-memory *only when security and ownership are correct*, lightweight context switches, explicit OOM handling and bounded crash recovery.
- Allocate memory based on **available physical RAM**, preserving a system reserve for kernel, services and framebuffer. Prevent user-space services, compositor, background jobs or AI features from monopolizing memory. Measure high-water marks, fragmentation, internal allocator overhead and shared-memory accounting; `alloc success` does not establish sustainability.
- Critical behavior on 512 MB: boot to a small interactive SLOPOS/SURYA desktop when that target exists, accept text/pointer input promptly, launch/close a tiny native app, retain focus and cancel/stage long disk or CPU work. Network and microSD slowness may delay task completion but must never lock all input or cause fabricated completion. Full modern graphics, 3D acceleration, large Loom documents or on-device ML cannot be promised within that RAM budget.
- Native port design for future ARM64: choose and document Pi-specific firmware or U-Boot boot path (do **not** assume x86 Limine UEFI, APIC or x86 interrupt ABI); ARMv8 exception levels, page tables, ASIDs, cache/TLB barriers, syscall/trap entry, timer/interrupt controller (including BCM2710's actual interrupt topology), multicore bring-up, MMIO/peripheral memory, mailbox/framebuffer, SD/storage, serial and video/input. Preserve architecture-neutral **DHARMA capability**, **VĀYU IPC**, **GARUDA supervisor**, **SURYA compositor** contracts rather than forking a second operating system. Never claim Linux driver code ports without kernel DMA/IRQ/firmware/DRM compatibility and license review (§5.2).
- **One graphical platform:** SLOPOS remains PARAMA's intended full desktop UI (per §5.1), with SURYA its only compositor. Pi's VideoCore IV means no assumed Vulkan/HDR/VRR/compositor GPU effects; require a minimal software/rendering path and honest optional feature fallback. Theme, icon and appkit semantics can be shared even if visual effects vary with hardware.

### Future board evidence and proposed edge targets

| Future ID | Required genuine evidence |
|---|---|
| ARM-00 | Freeze platform-specific boot/ABI/exception/timer/interrupt/device interfaces after an architecture review; boot minimal AArch64 kernel in QEMU virt **with explicitly labeled limits** and prove fault/interrupt/serial behavior. |
| ARM-01 | Actual Pi Zero 2 W board boot, console/memory-map/interrupt/timer test, real 512 MB accounting, fault isolation and reproducible cold boots. No platform acceptance from QEMU virt alone. |
| ARM-02 | Correct user-ring (EL0), per-process address spaces, capability isolation, IPC, native ELF loading and user-mode driver interfaces with bounded allocation, negative tests and cleanup on ARM64. |
| ARM-03 | SURYA on actual VideoCore IV/display path or verified framebuffer fallback: visible pointer/keyboard/surfaces, real input→frame events, no idle redraw storm, service restart and denied capability paths. |
| ARM-04 | Native SLOPOS small-shell/app workflow with classic/modern theme tokens, measured idle/resident memory, frame/input p95, low-memory pressure and shutdown/restart evidence. Other capability tiers must degrade truthfully. |

**Provisional usability aspirations pending hardware baseline, not acceptance claims:** once a genuine native desktop exists, at a documented **1280×720** configuration preserve sufficient free memory for an additional lightweight app (candidate **≥128 MiB free/available**, board-specific definition to be validated), target **p95 ≤150 ms** simple local input-to-visible response under light load, and avoid sustained busy polling, continuous idle frames, OOM and uncontrolled swap. Record cold/warm attempts with exact firmware, clock, temperature, display resolution, DMA/GPU reservation, backing storage, build SHA, serial trace and real device memory statistics. A future measured limit may replace these provisional hypotheses without erasing failed evidence. No fabricated "supports Pi" labels.

**Governance:** ARM-00–04 are a **separately authorized future architecture/board port**. Work on x86-64 M0/M1 first; never edit x86 boot/linker/ABI simply to shoehorn ARM or let speculative porting invalidate existing isolation/security tests. If no Pi hardware is available for verification, mark `NOT_RUN/BLOCKED` rather than replacing board proof with emulation.

### 5.4 2027 hardware, host and toolchain realities

**Planning notes recorded 2026-10-25; not milestones.** They keep later gates honest about what 2027 machines and hosts actually look like.

- **Input and buses on real hardware:** most current laptops and many desktops have no PS/2 controller, or only firmware emulation of one. Real-hardware input therefore needs an xHCI USB host driver and USB HID. PS/2 stays the VM path for M6. Interrupt routing, SMP bring-up and timers need ACPI (MADT, I/O APIC, HPET or the TSC deadline timer). Evaluate an existing ACPI implementation, such as ACPICA or uACPI, under §5.2's license and reuse rules rather than writing an AML interpreter from scratch.
- **First standard drivers:** in QEMU, virtio devices (virtio-gpu, virtio-input, virtio-blk, virtio-net) are small, well-specified, DMA-capable targets. They are good first proofs for capability-scoped drivers once §2.7's DMA rule can be met. NVMe is the common real-hardware storage target. VirtualBox offers different virtual devices (for example VMSVGA graphics), so each host's device set is recorded separately.
- **Windows 11 QA host:** when Hyper-V, WSL2 or Windows "memory integrity" (VBS) is enabled, VirtualBox runs on top of the Windows Hypervisor Platform. This changes timing and performance, and sometimes behavior. Record which mode the QA host used with every VirtualBox result.
- **Toolchain pinning:** M0 currently uses the Clang/LLD 18 shipped by Ubuntu 24.04. Ubuntu 26.04 LTS ships newer compilers. Pin the exact LLVM version and the container image digest for CI and Docker builds (§3.3 already notes the digest gap), and treat a compiler upgrade as its own change, verified with the reproducible-hash check from M0. Clang 18 accepts `-std=c23`, so moving the kernel flag from `-std=c2x` is a small, separate build change.
- **Boot path:** Limine stays pinned and checksummed (§6). Before relying on Limine features beyond the base protocol, such as Secure Boot support (§2.7), verify them for the pinned version.

## 6. Dependency, testing and governance rules

- Keep Limine binary **pinned** and verified. The bundled minimal protocol
  header is 0BSD; replacing it with the full official `limine.h` is encouraged
  when making protocol features more extensive, with a pinned source snapshot
  and preserved notice.
- MIT applies only to authored PARAMA code. `doomgeneric` is GPL-licensed;
  determine redistribution and linkage obligations, provenance and alternatives
  before combining it with other sources. Freedoom WADs have separate licensing.
- Use generated interfaces only if checked in and versioned. Explicitly verify
  third-party source checksum and license in any future build asset fetch.
- Maintain deterministic startup milestones and fail-closed validation. Do not
  pass VM tests by modifying regexes, ignoring errors or printing unconditional
  success while the associated work is missing.
- Security reports use GitHub private vulnerability reporting (enabled; Security tab → Report a vulnerability).
- Nightly: `.github/workflows/nightly.yml` publishes `parama-nightly-<sha>.iso`, the kernel ELF and checksums to the rolling `nightly` pre-release, but only after the QEMU/OVMF M1 boot gate passes. PR CI boots each mode 3 times; main runs the full 10-boot gate.
- Any agent must state `DONE`, `PARTIAL`, `BLOCKED` against real evidence.
  The integrator, not an isolated module author, updates the project truth table.

## 7. Next implementation request for parallel agents

**First mission:** complete M0/M1 VirtualBox gate. Run the pinned Limine ISO
build on Windows/WSL, fix only actual observed boot/build defects, keep COM1
logs and screenshot, then repeat 10 cold boots. Once green, partition M2 into
`PMM free/reclaim`, `paging`, `GDT/IDT + fault diagnostics`, and `timer plumbing`
with interface-first PRs and independent host unit tests. Do not work on SURYA
or YAMA until the Ring 3 and IPC gates are real.

---

## Future optional SLOPOS Intelligence service (2026-10-28 addition)

**Status: PLAN. Not a PARAMA feature at M1.** The authoritative detailed
integration contract is [docs/intelligence-integration.md](docs/intelligence-integration.md).
The provisional host-testable C23 frame contract is
[`include/parama/abi/intelligence.h`](include/parama/abi/intelligence.h).
Its framing validator checks lengths and versions **only**; neither the model
nor this header grants authority. This extension does not change the current
BRAHMA kernel build or promote the M3–M7 milestones.

At the earliest after M4/M5, GARUDA can supervise an unprivileged, opt-in
assistant service using VAYU and a separate action broker holding attenuated
DHARMA capabilities. The model may propose a typed app intent, but the broker
must validate target identity, per-app scopes, user consent, cancellation,
revocation and result boundaries. Sensitive context sources (files, microphone,
screen, messages, notifications) each require independent grants. Personal
content must not be sent to cloud backends without explicit permission.
Never let a model supply executable paths, arbitrary commands, raw pointers,
capability handles, or system calls. No AI inference, LFM weights, audio runtime
or large model allocator belongs in the kernel. On the Pi Zero 2 W, the service
must remain absent or use an explicit external backend; ARM64 port and memory
budgets still need actual validation.

Host ABI unit tests are a **CODE-only** result; the M4 kernel IPC, Ring 3 broker,
trusted confirmation UI, and end-to-end action tests remain entirely unbuilt.
