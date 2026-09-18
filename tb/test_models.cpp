#include <cstdio>
#include <cstdint>
#include "models.h"

int main() {
    int fails = 0;
    uint32_t s1 = FW_SEED, s2 = FW_SEED;
    int16_t a1[64], b1[64], a2[64], b2[64];
    gen_vectors(&s1, a1, b1, 64);
    gen_vectors(&s2, a2, b2, 64);
    for (int i = 0; i < 64; i++) if (a1[i] != a2[i] || b1[i] != b2[i]) fails++;
    if (s1 != s2) fails++;
    int16_t x[4] = {1, -2, 3, 4}, y[4] = {5, 6, -7, 8};
    if (dot_ref(x, y, 4) != (5 - 12 - 21 + 32)) fails++;
    int16_t big[64], neg[64];
    for (int i = 0; i < 64; i++) { big[i] = -32768; neg[i] = -32768; }
    if (dot_ref(big, neg, 64) != (int64_t)64 * 1073741824LL) fails++;
    if (to48(-1) != 0x0000FFFFFFFFFFFFull) fails++;
    printf("%s test_models checks=5 fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
