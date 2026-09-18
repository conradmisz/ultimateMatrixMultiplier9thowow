#include <cstdio>
#include <cstdint>
#include "models.h"

static int g_checks = 0, g_fails = 0;
static void check(bool cond, const char *msg) {
    ++g_checks;
    if (!cond) { ++g_fails; printf("  CHECK FAILED: %s\n", msg); }
}

// Independently transcribed xorshift32 (13/17/5), not calling models.h's xorshift32, so this
// test catches an accidental change to the header's algorithm rather than just re-running it.
static uint32_t ref_xorshift(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

int main() {
    // Known-answer test: first 8 xorshift32 outputs for seed FW_SEED, against a local
    // reimplementation of the 13/17/5 algorithm.
    uint32_t s_ref = FW_SEED, s_hdr = FW_SEED;
    uint32_t first_two[2];
    for (int i = 0; i < 8; i++) {
        uint32_t r = ref_xorshift(&s_ref);
        uint32_t h = xorshift32(&s_hdr);
        if (i < 2) first_two[i] = r;
        check(r == h, "xorshift32 step mismatch vs ref_xorshift");
    }

    // gen_vectors must draw a[0], a[1] (the first two calls) as the low 16 bits of the same
    // reference stream.
    int16_t a[64], b[64];
    uint32_t s_gen = FW_SEED;
    gen_vectors(&s_gen, a, b, 64);
    check(a[0] == (int16_t)(first_two[0] & 0xFFFF), "gen_vectors a[0] mismatch");
    check(a[1] == (int16_t)(first_two[1] & 0xFFFF), "gen_vectors a[1] mismatch");

    int16_t x[4] = {1, -2, 3, 4}, y[4] = {5, 6, -7, 8};
    check(dot_ref(x, y, 4) == (5 - 12 - 21 + 32), "dot_ref small case");

    int16_t big[64], neg[64];
    for (int i = 0; i < 64; i++) { big[i] = -32768; neg[i] = -32768; }
    check(dot_ref(big, neg, 64) == (int64_t)64 * 1073741824LL, "dot_ref extreme case");

    check(to48(-1) == 0x0000FFFFFFFFFFFFull, "to48(-1)");

    printf("%s test_models checks=%d fails=%d\n", g_fails ? "FAIL" : "PASS", g_checks, g_fails);
    return g_fails ? 1 : 0;
}
