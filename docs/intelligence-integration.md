# Future SLOPOS Intelligence integration on PARAMA

**Status: DESIGN ONLY (M1).** This repository does not yet have a functioning
Ring 3 process, DHARMA capability system or VAYU IPC implementation, so no
desktop AI daemon can execute on PARAMA today. Do not link inference or a
model runtime into BRAHMA.

The host-compileable `include/parama/abi/intelligence.h` and
`tools/tests/test_intelligence_abi.c` define and validate a **provisional**
24-byte, little-endian, versioned service payload envelope. It is carried as
the body of a future VAYU IPC message; its size including this header is at
most 4096 bytes. The header is **not** a capability handle or authentication.
Do not freeze it as the kernel syscall ABI or infer that a passing host test
proves end-to-end IPC.

## Required isolation and service design

1. After M3–M5, GARUDA may supervise a separate unprivileged intelligence
   service. BRAHMA remains unaware of models, prompts and application
   semantics. On 512 MiB systems, disable it by default and allow remote
   inference only after explicit opt-in.
2. The service can **propose**, never directly exercise, an app intent.
   Only a capability-holding app broker may resolve a versioned action schema,
   check the requesting identity and target, request per-action consent when
   necessary, and invoke the target through DHARMA/VAYU.
3. Query/revoke of permissions, cancellation and bounded result verification
   belong to the broker. UI confirmation is delivered by a trusted surface
   rather than drawing model-provided text as an approval prompt.
4. Restrict data access to explicitly granted resources. Documents, screen
   pixels, microphones, notifications and personal information are all
   separate grants. Logging must redact content; retain no transcript unless
   the user opts in.
5. No shared-memory mapping, DMA or driver capability is automatically
   inherited by AI services. Keep buffers bounded and drop unknown versions,
   opcodes, flags and payload lengths.
6. Compatibility adapter to SLOPOS's Linux-side action registry must translate
   typed actions, **not** Linux command lines or .desktop Exec fields, into
   PARAMA native app intents.

## Acceptance gates (not yet passed)

- Host ABI validation (layout, version, opcodes, lengths, overflow boundary)
- M4 kernel-mediated IPC with separate client/server task identities
- Negative capability tests: forged endpoint, revoked grant, cross-app access
- Trusted consent UI and cancellation verified while the service is blocked
- Native PARAMA backend interface, model available/unavailable behavior,
  memory budget and server restart behavior
- VM boot screenshots + serial evidence demonstrating a real end-to-end
  SLOPOS-native action, once M7 desktop is in place

The Linux implementation is tracked separately in
[the SLOPOS repository](https://github.com/palaashatri/rust-slopos/blob/feature/local-intelligence-v1/docs/intelligence.md).
