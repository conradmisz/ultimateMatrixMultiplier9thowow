#include "Vadder_tree.h"
#include "harness.h"
#include "memmap.h"

int main(int argc, char **argv) {
    Args args(argc, argv);
    Verilated::commandArgs(argc, argv);
    Harness<Vadder_tree> h(args, "adder_tree");
    h.reset();
    Rng rng(args.seed);
    for (int i = 0; i < args.iters + 3; i++) {
        int32_t v[P_LANES];
        int64_t exp = 0;
        for (int l = 0; l < P_LANES; l++) {
            if (i == 0) v[l] = 0x3FFFFFFF;            // max positive product
            else if (i == 1) v[l] = -0x40000000;      // min negative product
            else if (i == 2) v[l] = (l & 1) ? 1 : -1;
            else v[l] = (int32_t)rng.i16() * (int32_t)rng.i16();
            h.top.in[l] = (uint32_t)v[l];
            exp += v[l];
        }
        h.tick();
        int64_t got = (int64_t)h.top.sum;                 // 34-bit value in a 64-bit field
        if (got & (1ll << 33)) got -= (1ll << 34);         // sign-extend from 34 bits
        CHECK(got == exp, "iter %d got=%lld exp=%lld", i, (long long)got, (long long)exp);
    }
    return h.finish("adder_tree", args.seed);
}
