#include "Vram32.h"
#include "harness.h"
#include "bus.h"
#include <cstring>

int main(int argc, char **argv) {
    Args args(argc, argv);
    Verilated::commandArgs(argc, argv);          // passes +hex= through
    Harness<Vram32> h(args, "ram32");
    h.reset();
    Rng rng(args.seed);
    const int WORDS = 1024;                       // 4 KB test instance (see Makefile -G)
    uint32_t model[WORDS];

    // Preloaded contents from +hex=tb/data/ram_test.hex
    CHECK(bus_read(h, 0x0) == 0xdeadbeef, "hex word 0");
    CHECK(bus_read(h, 0x4) == 0x00000001, "hex word 1");
    CHECK(bus_read(h, 0x8) == 0xcafef00d, "hex word 2");
    CHECK(bus_read(h, 0xC) == 0x0, "hex word 3");
    model[0] = 0xdeadbeef; model[1] = 1; model[2] = 0xcafef00d; model[3] = 0;

    // Fill the rest with full-word writes, then random strobed writes and readback.
    for (int w = 4; w < WORDS; w++) { uint32_t v = rng.next(); bus_write(h, 4 * w, v); model[w] = v; }
    for (int i = 0; i < args.iters; i++) {
        uint32_t w = rng.below(WORDS), d = rng.next(); uint8_t s = rng.below(16);
        bus_write(h, 4 * w, d, s);
        for (int k = 0; k < 4; k++) if (s & (1 << k)) { model[w] &= ~(0xFFu << (8*k)); model[w] |= d & (0xFFu << (8*k)); }
        CHECK(bus_read(h, 4 * w) == model[w], "word %u after strb %x", w, s);
    }
    for (int w = 0; w < WORDS; w += 37) CHECK(bus_read(h, 4 * w) == model[w], "sweep word %u", w);
    return h.finish("ram32", args.seed);
}
