#pragma once
#include "harness.h"

// Drives the shared peripheral interface: bus_sel, bus_addr, bus_wdata, bus_wstrb -> bus_rdata, bus_ready.
// A transaction is accepted on the cycle where sel && !ready; ready pulses for one cycle after.
template <class H>
uint32_t bus_read(H &h, uint32_t addr) {
    auto &t = h.top;
    t.bus_sel = 1; t.bus_addr = addr & 0xFFFF; t.bus_wstrb = 0; t.bus_wdata = 0;
    int n = 0;
    do { h.tick(); ++n; } while (!t.bus_ready && n < 16);
    CHECK(t.bus_ready, "bus_read timeout at 0x%x", addr);
    uint32_t d = t.bus_rdata;
    t.bus_sel = 0;
    h.tick();
    CHECK(!t.bus_ready, "ready did not drop after read at 0x%x", addr);
    return d;
}

template <class H>
void bus_write(H &h, uint32_t addr, uint32_t data, uint8_t strb = 0xF) {
    auto &t = h.top;
    t.bus_sel = 1; t.bus_addr = addr & 0xFFFF; t.bus_wstrb = strb; t.bus_wdata = data;
    int n = 0;
    do { h.tick(); ++n; } while (!t.bus_ready && n < 16);
    CHECK(t.bus_ready, "bus_write timeout at 0x%x", addr);
    t.bus_sel = 0; t.bus_wstrb = 0;
    h.tick();
    CHECK(!t.bus_ready, "ready did not drop after write at 0x%x", addr);
}
