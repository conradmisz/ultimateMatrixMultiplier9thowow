#include "Vbus_decoder.h"
#include "harness.h"
#include "memmap.h"

int main(int argc, char **argv) {
    Args args(argc, argv);
    Verilated::commandArgs(argc, argv);
    Harness<Vbus_decoder> h(args, "bus_decoder");
    auto &t = h.top;
    t.mem_valid = 0; t.mem_addr = 0; t.mem_wdata = 0; t.mem_wstrb = 0;
    for (int i = 0; i < 7; i++) t.p_rdata[i] = 0x100 + i;
    t.p_ready = 0;
    h.reset();
    Rng rng(args.seed);

    const uint32_t bases[7] = {BASE_INSTR, BASE_DATA, BASE_SPA, BASE_SPB, BASE_DOTP, BASE_FIFO, BASE_GPIO};

    // Each mapped region: sel fires, offset passes through, rdata/ready come from that region.
    for (int r = 0; r < 7; r++) {
        uint32_t off = rng.next() & 0xFFFC;
        t.mem_valid = 1; t.mem_addr = bases[r] + off; t.mem_wdata = 0xA5A5A5A5; t.mem_wstrb = 0xF;
        t.eval();
        CHECK(t.sel == (1u << r), "region %d sel=%x", r, t.sel);
        CHECK(t.p_addr == off, "region %d p_addr=%x exp %x", r, t.p_addr, off);
        CHECK(t.p_wdata == 0xA5A5A5A5 && t.p_wstrb == 0xF, "passthrough region %d", r);
        CHECK(t.mem_ready == 0, "ready before peripheral responds");
        h.tick();
        t.p_ready = (1u << r); t.eval();
        CHECK(t.mem_ready == 1, "ready mux region %d", r);
        CHECK(t.mem_rdata == 0x100u + r, "rdata mux region %d got %x", r, t.mem_rdata);
        h.tick();
        t.p_ready = 0; t.mem_valid = 0; t.eval();
        CHECK(t.sel == 0, "sel must drop with valid");
    }
    // Unmapped addresses: no sel, ready pulse after one cycle, rdata 0, ready drops.
    const uint32_t bad[] = {0x00070000u, 0x000F0000u, 0x00100000u, 0x80000000u, 0xFFFFFFFCu};
    for (uint32_t a : bad) {
        t.mem_valid = 1; t.mem_addr = a; t.mem_wstrb = 0; t.eval();
        CHECK(t.sel == 0, "unmapped %x asserted sel %x", a, t.sel);
        CHECK(t.mem_ready == 0, "unmapped %x ready too early", a);
        h.tick();
        CHECK(t.mem_ready == 1 && t.mem_rdata == 0, "unmapped %x ready/rdata", a);
        t.mem_valid = 0;
        h.tick();
        CHECK(t.mem_ready == 0, "unmapped ready did not drop");
    }
    // Random addresses: sel matches a software decode.
    for (int i = 0; i < args.iters; i++) {
        uint32_t a = rng.next();
        if (rng.below(2)) a &= 0x000FFFFF;
        t.mem_valid = 1; t.mem_addr = a; t.eval();
        uint32_t region = (a >> 16) & 0xF;
        uint32_t exp = ((a >> 20) == 0 && region < 7) ? (1u << region) : 0;
        CHECK(t.sel == exp, "addr %x sel %x exp %x", a, t.sel, exp);
        t.mem_valid = 0; h.tick(); h.tick();
    }
    return h.finish("bus_decoder", args.seed);
}
