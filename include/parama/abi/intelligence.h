/* PARAMA/SLOPOS Intelligence service payload v1.
 *
 * DESIGN CONTRACT ONLY: no running PARAMA assistant, kernel endpoint, Ring 3
 * service, capability policy or model backend exists at M1.
 *
 * This framing is the SERVICE payload carried inside a future VAYU IPC
 * message; it does not replace the VAYU 24-byte transport header. Every
 * message is bounded by VAYU's proposed 4096-byte inline payload limit.
 */
#ifndef PARAMA_ABI_INTELLIGENCE_H
#define PARAMA_ABI_INTELLIGENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PARAMA_INTELLIGENCE_ABI_VERSION UINT16_C(1)
#define PARAMA_INTELLIGENCE_MAX_FRAME UINT32_C(4096)
#define PARAMA_INTELLIGENCE_HEADER_BYTES UINT32_C(24)

enum parama_intelligence_opcode {
    PARAMA_INTELLIGENCE_QUERY = 1,    /* enumerate allowed actions */
    PARAMA_INTELLIGENCE_PROPOSE = 2,  /* untrusted model proposal */
    PARAMA_INTELLIGENCE_AUTHORIZE = 3,/* policy/consent decision */
    PARAMA_INTELLIGENCE_CANCEL = 4,   /* cancellable request */
    PARAMA_INTELLIGENCE_RESULT = 5    /* bounded structured result */
};

/* Little-endian on supported x86-64; explicit serialization required for
 * any future big-endian port. No pointers, capabilities or process identities
 * may be encoded as raw in-process addresses.
 */
struct parama_intelligence_header {
    uint16_t version;
    uint16_t opcode;
    uint32_t flags;
    uint64_t request_id;
    uint32_t payload_bytes;
    uint32_t reserved;
};

_Static_assert(sizeof(struct parama_intelligence_header) == 24,
               "intelligence v1 framing must be exactly 24 bytes");
_Static_assert(offsetof(struct parama_intelligence_header, request_id) == 8,
               "request ID wire offset must be stable");
_Static_assert(offsetof(struct parama_intelligence_header, payload_bytes) == 16,
               "payload length wire offset must be stable");

/* Host-level framing validator, NOT authorization.
 * actual_frame_bytes is the byte count received from kernel IPC, not a
 * model-controlled length. Compare without wrap-around. All state-changing
 * actions still require DHARMA capability checks and user policy consent.
 */
static inline bool parama_intelligence_header_valid(
    const struct parama_intelligence_header *header,
    size_t actual_frame_bytes)
{
    if (header == NULL
        || actual_frame_bytes < sizeof(*header)
        || actual_frame_bytes > PARAMA_INTELLIGENCE_MAX_FRAME
        || header->version != PARAMA_INTELLIGENCE_ABI_VERSION
        || header->request_id == 0
        || header->flags != 0
        || header->reserved != 0
        || header->opcode < PARAMA_INTELLIGENCE_QUERY
        || header->opcode > PARAMA_INTELLIGENCE_RESULT) {
        return false;
    }
    return (size_t)header->payload_bytes
        == actual_frame_bytes - sizeof(*header);
}

#endif
