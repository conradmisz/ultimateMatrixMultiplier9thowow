#pragma once
#include <verilated.h>
#include <verilated_fst_c.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

struct Args {
    uint32_t seed = 1;
    int iters = 100;
    bool trace = true;
    std::string hex = "firmware/firmware.hex";
    long max_cycles = 2000000;
    Args(int argc, char **argv) {
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];
            if (a == "--seed" && i + 1 < argc) seed = (uint32_t)strtoul(argv[++i], nullptr, 0);
            else if (a == "--iters" && i + 1 < argc) iters = atoi(argv[++i]);
            else if (a == "--no-trace") trace = false;
            else if (a == "--hex" && i + 1 < argc) hex = argv[++i];
            else if (a == "--max-cycles" && i + 1 < argc) max_cycles = atol(argv[++i]);
            else if (a[0] == '+') { /* verilator plusarg */ }
            else { fprintf(stderr, "unknown arg %s\n", a.c_str()); exit(2); }
        }
    }
};

static int g_checks = 0, g_fails = 0;
#define CHECK(cond, ...)                                                        \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!(cond)) {                                                          \
            ++g_fails;                                                          \
            if (g_fails <= 20) {                                                \
                printf("  CHECK FAILED %s:%d: ", __FILE__, __LINE__);           \
                printf(__VA_ARGS__);                                            \
                printf("\n");                                                   \
            }                                                                   \
        }                                                                       \
    } while (0)

// Simple LCG so tests are reproducible from --seed without <random> noise.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1) {}
    uint32_t next() { s = s * 1664525u + 1013904223u; return s; }
    uint32_t below(uint32_t n) { return next() % n; }
    int16_t i16() { return (int16_t)(next() >> 16); }
};

template <class M>
struct Harness {
    struct TraceInit { explicit TraceInit(bool on) { if (on) Verilated::traceEverOn(true); } };
    TraceInit _trace_init;
    M top;
    VerilatedFstC *tfp = nullptr;
    uint64_t cycle = 0;

    Harness(const Args &a, const char *name) : _trace_init(a.trace) {
        if (a.trace) {
            tfp = new VerilatedFstC;
            top.trace(tfp, 99);
            std::string path = std::string("sim/tb_") + name + ".fst";
            tfp->open(path.c_str());
        }
        top.clk = 0;
        top.rst_n = 0;
    }

    // Inputs set before tick() are sampled on the rising edge inside tick().
    void tick() {
        top.clk = 0; top.eval(); if (tfp) tfp->dump(cycle * 10);
        top.clk = 1; top.eval(); if (tfp) tfp->dump(cycle * 10 + 5);
        ++cycle;
    }

    void reset(int cycles = 3) {
        top.rst_n = 0;
        for (int i = 0; i < cycles; i++) tick();
        top.rst_n = 1;
        tick();
    }

    int finish(const char *name, uint32_t seed) {
        top.final();
        if (tfp) { tfp->close(); delete tfp; tfp = nullptr; }
        if (g_checks == 0) { printf("FAIL %s seed=%u: no checks executed\n", name, seed); return 1; }
        if (g_fails) { printf("FAIL %s seed=%u checks=%d fails=%d cycles=%llu\n", name, seed, g_checks, g_fails, (unsigned long long)cycle); return 1; }
        printf("PASS %s seed=%u checks=%d cycles=%llu\n", name, seed, g_checks, (unsigned long long)cycle);
        return 0;
    }
};
