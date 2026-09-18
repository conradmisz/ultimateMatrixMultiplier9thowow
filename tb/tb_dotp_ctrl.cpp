#include "Vdotp_ctrl.h"
#include "harness.h"
#include "bus.h"
#include "models.h"
#include "memmap.h"

struct Vec { int16_t a[N_ELEMS], b[N_ELEMS]; };

static uint64_t pack_row(const int16_t *v, int r) {
    uint64_t x = 0;
    for (int k = 0; k < P_LANES; k++) x |= (uint64_t)(uint16_t)v[P_LANES * r + k] << (16 * k);
    return x;
}

// Drives row_a/row_b from the current row_addr before every edge, like the scratchpads would.
template <class H> static void feed(H &h, const Vec &v) {
    int r = h.top.row_addr;
    h.top.row_a = pack_row(v.a, r);
    h.top.row_b = pack_row(v.b, r);
}
template <class H> static void tick_fed(H &h, const Vec &v) { feed(h, v); h.tick(); feed(h, v); }

// Runs one dot product through the bus, returns the pushed value and cycles from start to done.
template <class H> static uint64_t run_one(H &h, const Vec &v, int length, int *cycles, bool *pushed_once) {
    uint64_t pushed = 0; int npush = 0;
    bus_write(h, DOTP_LENGTH, length);
    // row_addr is not guaranteed 0 here (a partial-length run may have left it mid-count until
    // the start edge resets it), so feed row 0 explicitly rather than whatever feed() currently reads.
    h.top.row_a = pack_row(v.a, 0); h.top.row_b = pack_row(v.b, 0);
    bus_write(h, DOTP_CTRL, CTRL_START_BIT);
    int n = 0;
    while (!h.top.done && n < 200) {
        if (h.top.push_valid) { pushed = h.top.push_data; npush++; }
        tick_fed(h, v); n++;
    }
    CHECK(h.top.done, "timeout waiting for done (length %d)", length);
    if (h.top.push_valid) { pushed = h.top.push_data; npush++; }
    *cycles = n; *pushed_once = (npush == 1);
    return pushed;
}

int main(int argc, char **argv) {
    Args args(argc, argv);
    Verilated::commandArgs(argc, argv);
    Harness<Vdotp_ctrl> h(args, "dotp_ctrl");
    auto &t = h.top;
    t.row_a = 0; t.row_b = 0; t.fifo_overflow = 0;
    h.reset();
    Rng rng(args.seed);

    CHECK(bus_read(h, DOTP_STATUS) == 0, "status after reset");
    CHECK(bus_read(h, DOTP_LENGTH) == 0, "length after reset");
    bus_write(h, DOTP_LENGTH, 64);
    CHECK(bus_read(h, DOTP_LENGTH) == 64, "length readback");
    // LENGTH clamps on the full 32-bit written word, not just the low 7 bits.
    bus_write(h, DOTP_LENGTH, 65);
    CHECK(bus_read(h, DOTP_LENGTH) == 64, "length clamp 65 -> 64");
    bus_write(h, DOTP_LENGTH, 128);
    CHECK(bus_read(h, DOTP_LENGTH) == 64, "length clamp 128 -> 64");
    bus_write(h, DOTP_LENGTH, 0xFFFFFFFFu);
    CHECK(bus_read(h, DOTP_LENGTH) == 64, "length clamp 0xFFFFFFFF -> 64");
    bus_write(h, DOTP_LENGTH, 0);
    CHECK(bus_read(h, DOTP_LENGTH) == 0, "length readback 0");
    bus_write(h, DOTP_LENGTH, 64);
    CHECK(bus_read(h, DOTP_LENGTH) == 64, "length readback 64");

    Vec v; int cyc; bool once;
    // 1. Full-length random vectors.
    for (int i = 0; i < args.iters; i++) {
        for (int k = 0; k < N_ELEMS; k++) { v.a[k] = rng.i16(); v.b[k] = rng.i16(); }
        uint64_t got = run_one(h, v, N_ELEMS, &cyc, &once);
        uint64_t exp = to48(dot_ref(v.a, v.b, N_ELEMS));
        CHECK(got == exp, "iter %d got %llx exp %llx", i, (unsigned long long)got, (unsigned long long)exp);
        CHECK(once, "expected exactly one push");
        CHECK(bus_read(h, DOTP_RESULT_LO) == (uint32_t)exp, "RESULT_LO");
        CHECK(bus_read(h, DOTP_RESULT_HI) == (uint32_t)(exp >> 32), "RESULT_HI");
        uint32_t st = bus_read(h, DOTP_STATUS);
        CHECK((st & STATUS_DONE_BIT) && !(st & STATUS_BUSY_BIT), "status after done: %x", st);
        if (i == 0) CHECK(cyc <= N_ELEMS / P_LANES + 6, "latency %d cycles too high", cyc);
    }
    // 2. Every partial length 0..64, including non-multiples of P. Elements past LENGTH are
    //    poisoned with large values so a lane-mask bug is visible.
    for (int len = 0; len <= N_ELEMS; len++) {
        for (int k = 0; k < N_ELEMS; k++) { v.a[k] = (k < len) ? rng.i16() : -32768; v.b[k] = (k < len) ? rng.i16() : -32768; }
        uint64_t got = run_one(h, v, len, &cyc, &once);
        CHECK(got == to48(dot_ref(v.a, v.b, len)), "length %d mismatch", len);
    }
    // 3. Extreme values: all -32768 pairs gives 64 * 2^30 = 2^36, positive, fits in 48 bits.
    for (int k = 0; k < N_ELEMS; k++) { v.a[k] = -32768; v.b[k] = -32768; }
    CHECK(run_one(h, v, N_ELEMS, &cyc, &once) == to48(dot_ref(v.a, v.b, N_ELEMS)), "extreme min*min");
    for (int k = 0; k < N_ELEMS; k++) { v.a[k] = -32768; v.b[k] = 32767; }
    CHECK(run_one(h, v, N_ELEMS, &cyc, &once) == to48(dot_ref(v.a, v.b, N_ELEMS)), "extreme min*max (negative)");
    // 4. Start while busy is ignored: only one push.
    for (int k = 0; k < N_ELEMS; k++) { v.a[k] = 1; v.b[k] = 1; }
    bus_write(h, DOTP_LENGTH, 64); feed(h, v);
    bus_write(h, DOTP_CTRL, CTRL_START_BIT);
    tick_fed(h, v);
    CHECK(bus_read(h, DOTP_STATUS) & STATUS_BUSY_BIT, "expected busy mid-run");
    bus_write(h, DOTP_CTRL, CTRL_START_BIT);        // second start while busy
    int pushes = 0; for (int n = 0; n < 60; n++) { if (t.push_valid) pushes++; tick_fed(h, v); }
    CHECK(pushes == 1, "start-while-busy produced %d pushes", pushes);
    CHECK(bus_read(h, DOTP_RESULT_LO) == 64, "1*1 x64");
    // 5. Overflow sticky: pulse in, read, W1C clears.
    t.fifo_overflow = 1; h.tick(); t.fifo_overflow = 0; h.tick();
    CHECK(bus_read(h, DOTP_STATUS) & STATUS_OVF_BIT, "overflow sticky not set");
    CHECK(bus_read(h, DOTP_STATUS) & STATUS_OVF_BIT, "read must not clear overflow sticky");
    bus_write(h, DOTP_STATUS, STATUS_OVF_BIT);
    CHECK(!(bus_read(h, DOTP_STATUS) & STATUS_OVF_BIT), "W1C did not clear overflow");
    // 5b. Coincident overflow pulse vs W1C clear on the same accept edge: the set must win.
    // Driven manually (not via bus_write/bus_read, which each tick internally) so fifo_overflow
    // is asserted on exactly the accept edge (bus_sel && !bus_ready).
    t.bus_sel = 1; t.bus_addr = DOTP_STATUS; t.bus_wstrb = 0xF; t.bus_wdata = STATUS_OVF_BIT;
    t.fifo_overflow = 1;
    h.tick();                              // accept edge: W1C clear and overflow set race here
    t.bus_sel = 0; t.bus_wstrb = 0; t.fifo_overflow = 0;
    h.tick();
    CHECK(bus_read(h, DOTP_STATUS) & STATUS_OVF_BIT, "coincident overflow must win over W1C clear");
    bus_write(h, DOTP_STATUS, STATUS_OVF_BIT);
    CHECK(!(bus_read(h, DOTP_STATUS) & STATUS_OVF_BIT), "W1C did not clear overflow after coincident case");
    // 6. Soft reset mid-run: no push, not busy, done clear, LENGTH kept.
    bus_write(h, DOTP_CTRL, CTRL_START_BIT); tick_fed(h, v); tick_fed(h, v);
    bus_write(h, DOTP_CTRL, CTRL_RESET_BIT);
    pushes = 0; for (int n = 0; n < 30; n++) { if (t.push_valid) pushes++; tick_fed(h, v); }
    CHECK(pushes == 0, "soft reset still pushed");
    CHECK(bus_read(h, DOTP_STATUS) == 0, "status after soft reset");
    CHECK(bus_read(h, DOTP_LENGTH) == 64, "LENGTH lost on soft reset");
    // 7. Runs again correctly after soft reset.
    CHECK(run_one(h, v, N_ELEMS, &cyc, &once) == 64, "run after soft reset");
    return h.finish("dotp_ctrl", args.seed);
}
