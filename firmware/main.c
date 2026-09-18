#include <stdint.h>
#include "memmap.h"
#include "dotp_ref.h"

_Static_assert(FW_ITERS <= FIFO_DEPTH, "FIFO cannot hold one result per iteration");

static int64_t ref[FW_ITERS];   /* .bss, in data RAM */

static inline void gpio_set(uint32_t v) { REG32(BASE_GPIO + GPIO_OUT) = v; }
static inline uint32_t gpio_get(void)   { return REG32(BASE_GPIO + GPIO_OUT); }

/* The only function allowed to touch FIFO_LO / FIFO_HI (reading HI pops). */
static uint64_t fifo_pop(void) {
    uint32_t lo = REG32(BASE_FIFO + FIFO_LO);
    uint32_t hi = REG32(BASE_FIFO + FIFO_HI);
    return ((uint64_t)hi << 32) | lo;
}

int main(void) {
    volatile int16_t *spa = (volatile int16_t *)(uintptr_t)BASE_SPA;
    volatile int16_t *spb = (volatile int16_t *)(uintptr_t)BASE_SPB;
    int16_t a[N_ELEMS], b[N_ELEMS];
    uint32_t seed = FW_SEED;
    uint32_t gp = 0;

    REG32(BASE_DOTP + DOTP_LENGTH) = N_ELEMS;

    for (int it = 0; it < FW_ITERS; it++) {
        gen_vectors(&seed, a, b, N_ELEMS);
        for (int i = 0; i < N_ELEMS; i++) { spa[i] = a[i]; spb[i] = b[i]; }
        ref[it] = dot_ref(a, b, N_ELEMS);
        REG32(BASE_DOTP + DOTP_CTRL) = CTRL_START_BIT;
        while (!(REG32(BASE_DOTP + DOTP_STATUS) & STATUS_DONE_BIT)) { }
        gp ^= GPIO_HEARTBEAT_BIT;
        gpio_set(gp);
    }

    int ok = 1;
    if (REG32(BASE_FIFO + FIFO_COUNT) != FW_ITERS) ok = 0;
    if (REG32(BASE_DOTP + DOTP_STATUS) & STATUS_OVF_BIT) ok = 0;
    for (int it = 0; it < FW_ITERS; it++) {
        uint64_t got = fifo_pop();
        uint64_t exp = (uint64_t)ref[it] & ACC_MASK;
        if (got != exp) ok = 0;
    }
    if (REG32(BASE_FIFO + FIFO_COUNT) != 0) ok = 0;

    gp |= ok ? GPIO_PASS_BIT : GPIO_FAIL_BIT;
    gpio_set(gp);
    gp |= GPIO_FINISHED_BIT;
    gpio_set(gp);
    (void)gpio_get();  /* exercises the GPIO read path */
    for (;;) { }
}
