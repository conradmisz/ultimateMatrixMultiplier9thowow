#include "Vresult_fifo.h"
#include "harness.h"
#include "bus.h"
#include "models.h"
#include "memmap.h"

static uint64_t rand48(Rng &r) { return ((uint64_t)r.next() << 32 | r.next()) & 0x0000FFFFFFFFFFFFull; }

// Push on one clock edge (accept semantics are irrelevant to push; it is a plain valid pulse).
template <class H> static void push(H &h, uint64_t v) {
    h.top.push_valid = 1; h.top.push_data = v; h.tick(); h.top.push_valid = 0;
}
template <class H> static uint64_t pop_read(H &h) {
    uint32_t lo = bus_read(h, FIFO_LO);
    uint32_t hi = bus_read(h, FIFO_HI);
    return ((uint64_t)hi << 32) | lo;
}

int main(int argc, char **argv) {
    Args args(argc, argv);
    Verilated::commandArgs(argc, argv);
    Harness<Vresult_fifo> h(args, "result_fifo");
    h.top.push_valid = 0; h.top.push_data = 0;
    h.reset();
    Rng rng(args.seed);
    FifoModel m(FIFO_DEPTH);

    CHECK(bus_read(h, FIFO_COUNT) == 0, "count after reset");
    CHECK(pop_read(h) == 0, "empty read returns 0");
    CHECK(bus_read(h, FIFO_COUNT) == 0, "pop on empty must not underflow");

    // Ordered push/pop with random interleaving.
    for (int i = 0; i < args.iters; i++) {
        if (rng.below(2) == 0 || m.count() == 0) {
            uint64_t v = rand48(rng);
            push(h, v); m.push(v);
            CHECK(h.top.overflow == 0, "unexpected overflow at count %u", m.count());
        } else {
            uint64_t exp = m.front(); m.pop();
            CHECK(pop_read(h) == exp, "pop order mismatch at iter %d", i);
        }
        CHECK(bus_read(h, FIFO_COUNT) == m.count(), "count mismatch iter %d", i);
        CHECK(h.top.count == m.count(), "count port mismatch iter %d", i);
    }
    // Drain.
    while (m.count()) { uint64_t exp = m.front(); m.pop(); CHECK(pop_read(h) == exp, "drain mismatch"); }

    // LO has no side effect: read LO three times, count unchanged.
    push(h, 0x123456789ABCull); m.push(0x123456789ABCull);
    bus_read(h, FIFO_LO); bus_read(h, FIFO_LO);
    CHECK(bus_read(h, FIFO_LO) == 0x56789ABCu, "LO value");
    CHECK(bus_read(h, FIFO_COUNT) == 1, "LO must not pop");
    CHECK(bus_read(h, FIFO_HI) == 0x1234u, "HI value");
    m.pop();
    CHECK(bus_read(h, FIFO_COUNT) == 0, "HI must pop");

    // Fill exactly, then overflow once.
    for (int i = 0; i < FIFO_DEPTH; i++) { uint64_t v = rand48(rng); push(h, v); m.push(v); CHECK(h.top.overflow == 0, "overflow while filling %d", i); }
    CHECK(bus_read(h, FIFO_COUNT) == FIFO_DEPTH, "full count");
    push(h, 0xDEAD);
    CHECK(h.top.overflow == 1, "overflow pulse missing");
    h.tick();
    CHECK(h.top.overflow == 0, "overflow must be a one-cycle pulse");
    CHECK(bus_read(h, FIFO_COUNT) == FIFO_DEPTH, "count changed on dropped push");
    for (int i = 0; i < FIFO_DEPTH; i++) { uint64_t exp = m.front(); m.pop(); CHECK(pop_read(h) == exp, "post-overflow drain %d", i); }
    CHECK(bus_read(h, FIFO_COUNT) == 0, "empty after drain");

    // Simultaneous push and pop on the same accept edge: count unchanged, order preserved.
    push(h, 0x1111); m.push(0x1111);
    push(h, 0x2222); m.push(0x2222);
    bus_read(h, FIFO_LO);
    h.top.bus_sel = 1; h.top.bus_addr = FIFO_HI; h.top.bus_wstrb = 0;
    h.top.push_valid = 1; h.top.push_data = 0x3333;
    h.tick();                               // accept edge: pop 0x1111 and push 0x3333
    h.top.push_valid = 0; h.top.bus_sel = 0;
    CHECK(h.top.bus_ready == 1, "ready after simultaneous op");
    CHECK((h.top.bus_rdata & 0xFFFF) == 0, "HI of 0x1111 is 0");
    h.tick();
    m.pop(); m.push(0x3333);
    CHECK(bus_read(h, FIFO_COUNT) == 2, "count after simultaneous push/pop");
    CHECK(pop_read(h) == 0x2222, "order after simultaneous op");
    CHECK(pop_read(h) == 0x3333, "order after simultaneous op 2");

    return h.finish("result_fifo", args.seed);
}
