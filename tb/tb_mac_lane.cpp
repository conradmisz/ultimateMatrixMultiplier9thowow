#include "Vmac_lane.h"
#include "harness.h"

int main(int argc, char **argv) {
    Args args(argc, argv);
    Verilated::commandArgs(argc, argv);
    Harness<Vmac_lane> h(args, "mac_lane");
    h.reset();
    Rng rng(args.seed);
    const int16_t corners[] = {0, 1, -1, 32767, -32768, 255, -256};
    int nc = sizeof(corners) / sizeof(corners[0]);
    for (int i = 0; i < args.iters + nc * nc; i++) {
        int16_t a, b;
        if (i < nc * nc) { a = corners[i / nc]; b = corners[i % nc]; }
        else { a = rng.i16(); b = rng.i16(); }
        h.top.a = (uint16_t)a; h.top.b = (uint16_t)b;
        h.tick();
        int32_t got = (int32_t)h.top.p;
        int32_t exp = (int32_t)a * (int32_t)b;
        CHECK(got == exp, "a=%d b=%d got=%d exp=%d", a, b, got, exp);
    }
    return h.finish("mac_lane", args.seed);
}
