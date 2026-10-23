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
registers, no triple-fault/reboot; no interrupts enabled prematurely.

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
- Any agent must state `DONE`, `PARTIAL`, `BLOCKED` against real evidence.
  The integrator, not an isolated module author, updates the project truth table.

## 7. Next implementation request for parallel agents

**First mission:** complete M0/M1 VirtualBox gate. Run the pinned Limine ISO
build on Windows/WSL, fix only actual observed boot/build defects, keep COM1
logs and screenshot, then repeat 10 cold boots. Once green, partition M2 into
`PMM free/reclaim`, `paging`, `GDT/IDT + fault diagnostics`, and `timer plumbing`
with interface-first PRs and independent host unit tests. Do not work on SURYA
or YAMA until the Ring 3 and IPC gates are real.
