#include "Vgpio.h"
#include "harness.h"
#include "bus.h"
#include "memmap.h"

int main(int argc, char **argv) {
    Args args(argc, argv);
    Verilated::commandArgs(argc, argv);
    Harness<Vgpio> h(args, "gpio");
    h.reset();
    CHECK(h.top.gpio_out == 0, "gpio_out not 0 after reset: %x", h.top.gpio_out);
    CHECK(bus_read(h, GPIO_OUT) == 0, "OUT not 0 after reset");

    Rng rng(args.seed);
    for (int i = 0; i < args.iters; i++) {
        uint32_t v = rng.next();
        bus_write(h, GPIO_OUT, v);
        CHECK(h.top.gpio_out == (v & 0xF), "gpio_out %x != %x", h.top.gpio_out, v & 0xF);
        CHECK(bus_read(h, GPIO_OUT) == (v & 0xF), "readback mismatch");
    }
    // Byte strobe: writing with strobe 0 must not change the register.
    bus_write(h, GPIO_OUT, 0xF);
    bus_write(h, GPIO_OUT, 0x0, 0x0);
    CHECK(h.top.gpio_out == 0xF, "strobe-0 write changed gpio_out");
    // Unused offset reads as 0 and does not hang.
    CHECK(bus_read(h, 0x40) == 0, "unused offset should read 0");
    // Reset clears.
    h.reset();
    CHECK(h.top.gpio_out == 0, "reset did not clear");
    return h.finish("gpio", args.seed);
}
