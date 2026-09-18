#include "Vsoc_top.h"
#include "harness.h"
#include "models.h"
#include "memmap.h"
#include <vector>

int main(int argc, char **argv) {
    Args args(argc, argv);
    Verilated::commandArgs(argc, argv);        // carries +hex=firmware/firmware.hex to $value$plusargs
    Harness<Vsoc_top> h(args, "soc_top");
    auto &t = h.top;
    h.reset(5);

    std::vector<uint64_t> results;
    long n = 0;
    int heartbeat_toggles = 0; uint32_t last_gpio = 0;
    while (!(t.gpio_out & GPIO_FINISHED_BIT) && n < args.max_cycles) {
        h.tick(); n++;
        CHECK(t.trap == 0, "core trapped at cycle %ld", n);
        if (t.trap) break;
        if (t.dbg_result_valid) results.push_back(t.dbg_result);
        if ((t.gpio_out ^ last_gpio) & GPIO_HEARTBEAT_BIT) heartbeat_toggles++;
        last_gpio = t.gpio_out;
    }
    CHECK(t.gpio_out & GPIO_FINISHED_BIT, "firmware did not finish within %ld cycles", args.max_cycles);
    CHECK(t.gpio_out & GPIO_PASS_BIT, "firmware reported FAIL (gpio=%x)", t.gpio_out);
    CHECK(!(t.gpio_out & GPIO_FAIL_BIT), "fail bit set (gpio=%x)", t.gpio_out);
    CHECK(heartbeat_toggles == FW_ITERS, "heartbeat toggled %d times, expected %d", heartbeat_toggles, FW_ITERS);
    CHECK(results.size() == (size_t)FW_ITERS, "captured %zu results, expected %d", results.size(), FW_ITERS);

    // Independent recomputation from the same seed and generation order as the firmware.
    uint32_t seed = FW_SEED; int16_t a[N_ELEMS], b[N_ELEMS];
    for (int it = 0; it < FW_ITERS && it < (int)results.size(); it++) {
        gen_vectors(&seed, a, b, N_ELEMS);
        uint64_t exp = to48(dot_ref(a, b, N_ELEMS));
        CHECK(results[it] == exp, "iteration %d hw=%llx ref=%llx", it, (unsigned long long)results[it], (unsigned long long)exp);
    }
    printf("  firmware finished in %ld cycles\n", n);
    return h.finish("soc_top", args.seed);
}
