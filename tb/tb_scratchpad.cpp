#include "Vscratchpad.h"
#include "harness.h"
#include "bus.h"
#include "memmap.h"
#include <cstring>

static uint64_t model_row(const uint8_t *mem, int r) {
    uint64_t v = 0;
    for (int k = 0; k < 8; k++) v |= (uint64_t)mem[8 * r + k] << (8 * k);
    return v;
}

int main(int argc, char **argv) {
    Args args(argc, argv);
    Verilated::commandArgs(argc, argv);
    Harness<Vscratchpad> h(args, "scratchpad");
    h.reset();
    Rng rng(args.seed);
    uint8_t mem[128];
    memset(mem, 0, sizeof mem);

    // 1. Fill as int16_t[64] using halfword writes (two byte strobes), like the firmware does.
    int16_t vec[64];
    for (int i = 0; i < 64; i++) vec[i] = rng.i16();
    for (int i = 0; i < 64; i++) {
        uint32_t off = 2 * i;
        uint32_t word = ((uint32_t)(uint16_t)vec[i]) << (8 * (off & 3));
        uint8_t strb = 0x3 << (off & 3);
        bus_write(h, off & ~3u, word, strb);
        mem[off] = vec[i] & 0xFF; mem[off + 1] = (vec[i] >> 8) & 0xFF;
    }
    // 2. Every row matches the packed layout.
    for (int r = 0; r < 16; r++) {
        h.top.row_addr = r;
        h.top.eval();
        uint64_t exp = model_row(mem, r);
        CHECK(h.top.row_data == exp, "row %d got %llx exp %llx", r, (unsigned long long)h.top.row_data, (unsigned long long)exp);
        for (int k = 0; k < 4; k++)
            CHECK((int16_t)((h.top.row_data >> (16 * k)) & 0xFFFF) == vec[4 * r + k], "row %d lane %d", r, k);
    }
    // 3. Word reads over the bus match.
    for (int w = 0; w < 32; w++) {
        uint32_t exp = mem[4*w] | mem[4*w+1] << 8 | mem[4*w+2] << 16 | (uint32_t)mem[4*w+3] << 24;
        CHECK(bus_read(h, 4 * w) == exp, "word %d", w);
    }
    // 4. Random strobed writes.
    for (int i = 0; i < args.iters; i++) {
        uint32_t off = rng.below(32) * 4;
        uint32_t data = rng.next();
        uint8_t strb = rng.below(16);
        bus_write(h, off, data, strb);
        for (int k = 0; k < 4; k++) if (strb & (1 << k)) mem[off + k] = (data >> (8 * k)) & 0xFF;
        uint32_t exp = mem[off] | mem[off+1] << 8 | mem[off+2] << 16 | (uint32_t)mem[off+3] << 24;
        CHECK(bus_read(h, off) == exp, "strobed write at %u strb %x", off, strb);
        int r = off / 8;
        h.top.row_addr = r; h.top.eval();
        CHECK(h.top.row_data == model_row(mem, r), "row %d after strobed write", r);
    }
    return h.finish("scratchpad", args.seed);
}
