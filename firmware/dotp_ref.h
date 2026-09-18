#ifndef DOTP_REF_H
#define DOTP_REF_H
#include <stdint.h>

#define FW_SEED  0x2545F491u
#define FW_ITERS 16

static inline uint32_t xorshift32(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/* Fills a[0..n-1] then b[0..n-1], in that order, one xorshift call per element. */
static inline void gen_vectors(uint32_t *s, int16_t *a, int16_t *b, int n) {
    for (int i = 0; i < n; i++) a[i] = (int16_t)(xorshift32(s) & 0xFFFFu);
    for (int i = 0; i < n; i++) b[i] = (int16_t)(xorshift32(s) & 0xFFFFu);
}

/* Products fit in int32 (|int16*int16| <= 2^30); the sum is accumulated in int64. */
static inline int64_t dot_ref(const int16_t *a, const int16_t *b, int n) {
    int64_t acc = 0;
    for (int i = 0; i < n; i++) acc += (int32_t)a[i] * (int32_t)b[i];
    return acc;
}

#endif
