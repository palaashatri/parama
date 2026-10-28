#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "parama/abi/intelligence.h"

int main(void) {
    struct parama_intelligence_header h = {
        .version = PARAMA_INTELLIGENCE_ABI_VERSION,
        .opcode = PARAMA_INTELLIGENCE_QUERY,
        .request_id = 1,
        .payload_bytes = 2
    };
    assert(sizeof(h) == PARAMA_INTELLIGENCE_HEADER_BYTES);
    assert(parama_intelligence_header_valid(&h, sizeof(h) + 2));
    assert(!parama_intelligence_header_valid(NULL, sizeof(h) + 2));
    assert(!parama_intelligence_header_valid(&h, sizeof(h) + 1));
    assert(!parama_intelligence_header_valid(&h, sizeof(h) + 3));
    assert(!parama_intelligence_header_valid(&h, sizeof(h) - 1));
    assert(!parama_intelligence_header_valid(&h, (size_t)PARAMA_INTELLIGENCE_MAX_FRAME + 1));

    h.version = 2;
    assert(!parama_intelligence_header_valid(&h, sizeof(h) + 2));
    h.version = PARAMA_INTELLIGENCE_ABI_VERSION;
    h.opcode = 99;
    assert(!parama_intelligence_header_valid(&h, sizeof(h) + 2));
    h.opcode = PARAMA_INTELLIGENCE_QUERY;
    h.request_id = 0;
    assert(!parama_intelligence_header_valid(&h, sizeof(h) + 2));
    h.request_id = 1;
    h.flags = 1;
    assert(!parama_intelligence_header_valid(&h, sizeof(h) + 2));
    h.flags = 0;
    h.reserved = 1;
    assert(!parama_intelligence_header_valid(&h, sizeof(h) + 2));
    h.reserved = 0;
    h.payload_bytes = PARAMA_INTELLIGENCE_MAX_FRAME - PARAMA_INTELLIGENCE_HEADER_BYTES;
    assert(parama_intelligence_header_valid(&h, PARAMA_INTELLIGENCE_MAX_FRAME));
    h.payload_bytes++;
    assert(!parama_intelligence_header_valid(&h, PARAMA_INTELLIGENCE_MAX_FRAME));
    puts("[UNIT][PASS] PARAMA intelligence service framing v1");
    return 0;
}
