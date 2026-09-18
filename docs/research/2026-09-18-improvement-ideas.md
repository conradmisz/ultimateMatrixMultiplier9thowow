# Improvement Ideas, Ranked by Difficulty

2026-09-18. Written against the v1 design in `docs/systemSpec.md` before any RTL exists.
None of these are in scope for v1; this is a menu for after `make test` passes.

## Where the time actually goes

Per iteration, rough cycle budget at PicoRV32's default ~4 CPI:

| Phase                                  | Cycles (approx) | Share |
| -------------------------------------- | --------------- | ----- |
| Generate 128 values and store them     | 2,000 - 3,000   | ~40%  |
| Software reference (rv32im, native mul)| 1,500 - 2,500   | ~35%  |
| Software reference (rv32i, `__mulsi3`) | 10,000+         | n/a   |
| Accelerator, start to done             | ~20             | <1%   |
| Poll loop, heartbeat, loop overhead    | 100 - 300       | ~5%   |

Consequence: every performance lever that matters is on the core / bus / firmware side.
The accelerator is already 100x faster than the thing feeding it. "Pipelining" splits into
four different items with very different costs (see P1, P2, M4, H1 below).

## Tier T: trivial (minutes to an hour, no new modules)

| # | Idea | Benefit |
|---|------|---------|
| T1 | **PicoRV32 parameter tuning**: `ENABLE_MUL=1`, `ENABLE_FAST_MUL=1`, `BARREL_SHIFTER=1`, `TWO_STAGE_SHIFT=0`, `ENABLE_REGS_DUALPORT=1`, `TWO_CYCLE_ALU=0`, `ENABLE_COUNTERS=1`. | `FAST_MUL` turns the sequential 32+ cycle multiplier into a single-cycle one, so the software reference drops several-fold. Barrel shifter helps xorshift. Biggest cycles-per-hour win in the project. Cost: LEs/DSPs on a 176K LE part, irrelevant. |
| T2 | **Word-packed stores in firmware**: xorshift32 already yields 32 bits; store one packed word (`b<<16 \| a`) instead of two halfword stores. | Halves scratchpad-fill bus transactions. Keep the reference loop on the same packed words so generation and reference share one pass. |
| T3 | **Reorder firmware**: fill, start accelerator, *then* compute the reference, then check done. | Hides the accelerator entirely behind the reference computation. Free, and the right shape for any future faster core. |
| T4 | **PARAMS / ID register in `dotp_ctrl`** (read-only: N, P, ELEM_BITS, FIFO_DEPTH, version). | Firmware asserts at boot that `memmap.h` matches `soc_pkg.sv`. Turns architecture Invariant 2 from a convention into a runtime check. |
| T5 | **Sim console register** (e.g. `0x0007_0000` PUTC): testbench prints the byte; on the board it becomes a UART later. | Firmware can print per-iteration results and cycle counts. Massively speeds up debugging `tb_soc_top`. |
| T6 | **Cycle reporting**: with `ENABLE_COUNTERS`, firmware reads `rdcycle` around each phase and writes them to the console or a data RAM table the testbench dumps. | Makes every later optimisation measurable. Do this before T1 so there is a before/after number. |
| T7 | **Bus fault latch**: decoder records the address of the last unmapped access in a sticky register (still returns ready + 0). | Catches wrong `memmap.h` constants and stray pointers without breaking Invariant 3. |
| T8 | **STATUS extras**: fifo_full, fifo_empty, and a LENGTH-out-of-range error bit (0 or >N rejects the start). | Removes silent misbehaviour. A few lines. |
| T9 | **Verilator `--assert` + immediate assertions** in RTL (`ifndef SYNTHESIS`), e.g. FIFO never pops empty, row index < N/P, one-hot decoder select. | Bugs fail at the cause instead of at the golden-model diff. |
| T10 | **Verilator `--coverage`** and a `make cov` target. | Proves LENGTH<N, overflow, and every decoder region are actually hit. Interview-panel gold. |

## Tier E: easy (a module-level change, half a day)

| # | Idea | Benefit |
|---|------|---------|
| E1 | **Zero-wait-state RAM via PicoRV32's look-ahead port** (`mem_la_read`, `mem_la_addr`). Address the RAM from the look-ahead signals so read data is valid in the same cycle `mem_valid` rises. | Removes one cycle from every fetch, load, and store. Fetches dominate, so this is roughly a 20-30% cycle cut across all firmware phases. Only `ram_instr` / `ram_data` need it. |
| E2 | **Compressed ISA** (`COMPRESSED_ISA=1`, `-march=rv32imc`). | Smaller code means fewer instruction fetches over the bus. Pairs with E1. Check the GCC build supports it (first-unit toolchain check). |
| E3 | **Register the DSP path properly**: registered row read, registered A/B inputs, registered product, then the tree. Both operands `$signed`. Do not reset the product/pipeline registers or the memory arrays. | Lets Efinity absorb the pipeline registers into the DSP block's input/output registers and infer BRAM cleanly. This is the Fmax lever; expect it to be the difference between ~100 MHz and ~250 MHz on the Ti180. |
| E4 | **Scratchpad as two 32-bit simple-dual-port memories** (even word / odd word) rather than one 64-bit array with byte lanes. | Mixed-width ports (32-bit write, 64-bit read) are a common BRAM-inference failure. Two identical 32-bit memories always infer. Same behaviour on the bus and the private port. |
| E5 | **Trace only on failure / on request** (`--trace` gated by a flag, re-run with trace when a seed fails). | `make test` gets several times faster once tests grow. |
| E6 | **CI**: GitHub Action running `make lint` and `make test` on Ubuntu with Verilator + riscv gcc from apt. | Keeps the repo green while the docs and RTL are edited by agents. |

## Tier M: medium (a new module or a datapath restructure, a day or two)

| # | Idea | Benefit |
|---|------|---------|
| M1 | **Throughput mode: hardware vector fill**. A seeded LFSR/xorshift in `dotp_ctrl` fills both scratchpads itself; `CTRL.bit2 = hw_fill`, plus a `COUNT` register to run K dot products back to back with no core involvement. Firmware spot-checks by reading back one vector pair and its FIFO entry. | The only change that makes the accelerator the bottleneck and lets you quote real throughput (64-element product every 16 cycles, i.e. ~1.2 G MAC/s at 300 MHz). Already noted as a stretch in D8. |
| M2 | **Per-lane accumulate instead of an adder tree in the loop**: each lane does `acc_p <= acc_p + a*b` (one DSP with its own accumulator), and a single 4-input reduce runs once at the end. | Maps each lane to exactly one DSP block using its native accumulator; the adder tree leaves the per-cycle critical path. Cleaner timing, less logic. Changes the `adder_tree` module's role, so record it in `decisions.md`. |
| M3 | **Command FIFO on the input side** (LENGTH + start queued, mirroring the result FIFO). | Core can enqueue several products and walk away. Only pays off with M1 or ping-pong buffers, but makes the two FIFOs symmetric and easy to explain. |
| M4 | **Overlapped dot products**: accept the next start while the tail of the previous one is still draining the tree; needs a per-product tag through the pipeline. | Saves the ~4-cycle drain bubble per product. Worthless at 16 iterations, real in throughput mode (M1), where it is a ~20% gain. |
| M5 | **Ping-pong scratchpads** (A0/B0, A1/B1, bank-select bit in CTRL). | Lets the core fill bank 1 while the accelerator reads bank 0. Honest assessment: with compute at 20 cycles and fill at 2,000, this hides nothing. Only worth it if N grows (N=1024+) or with a faster core. Listed because reviewers expect the question. |
| M6 | **Parametrise P and N end to end** (generate-based adder tree, tests run at P=2/4/8, N=32/64/128). | Demonstrates scalability and finds hard-coded assumptions. Firmware `memmap.h` must be generated from `soc_pkg.sv` or vice versa (a small script), which also nails Invariant 2. |
| M7 | **Interrupt on done** (`ENABLE_IRQ=1`, one IRQ line from `dotp_ctrl`, W1C). | Replaces the poll loop. Low value here (20-cycle latency) but it is the standard "what would you add next" answer and PicoRV32's IRQ is simple. Currently explicitly out of scope. |
| M8 | **Formal check of `result_fifo` and `bus_decoder`** with SymbiYosys. | Exhaustive proof of the pop-on-RING_HI-only invariant. Toolchain on macOS is the pain, not the proofs. |

## Tier H: hard (architecture-changing)

| # | Idea | Benefit |
|---|------|---------|
| H1 | **Pipelined core**: replace PicoRV32 with a 5-stage core (VexRiscv generated Verilog, or Ibex). | PicoRV32 is deliberately multi-cycle (~4 CPI). A pipelined core is ~1.1 CPI: a 3-4x cut on the phases that hold 99% of the runtime. This is the single largest performance lever, and also the most expensive: it reverses D2, needs a new bus adapter, and Invariant 7 goes away. Do it only if throughput of the *firmware* becomes the story. |
| H2 | **AXI4-Lite wrapper on the accelerator** (the D3 stretch). | Makes the accelerator droppable into Sapphire or any vendor SoC. Zero performance benefit; purely portability and a talking point. |
| H3 | **Separate accelerator clock domain** via PLL with CDC on the FIFO. | Lets the DSP path run at its Fmax independent of the core. Not worth it until M1 exists and the core clock is proven to limit the accelerator. |

## Suggested order if you only do a few

1. T6, then T1, T2, T3: measurable, free, and they attack the actual bottleneck.
2. E1 and E3: the two real hardware-side wins (bus latency, Fmax).
3. T4, T5, T9, T10: the "this person verifies things" signals for the panel.
4. M1 (+ M2, M4): turns the design from a demo into something with a throughput number.

## Not recommended

- DMA / burst fill engine: the core generates the data, so nothing can copy it faster than it
  is produced. M1 is the right answer to the same problem.
- Wider bus: PicoRV32 is a 32-bit master; a 64-bit bus buys nothing.
- Saturating accumulator: 38 bits needed, 48 available. Overflow is impossible.

---

# Deep dive: the "if you only do a few" set

## 1. Cycle reporting (T6)

**Mechanism.** `ENABLE_COUNTERS=1` exposes `rdcycle` / `rdcycleh` / `rdinstret` CSRs.
Firmware samples `rdcycle` at phase boundaries (fill A, fill B, reference, accelerator wait,
drain, compare) and reports the deltas. `rdinstret` alongside gives CPI directly, which is
the number that shows what the PicoRV32 tuning (T1) and look-ahead RAM (E1) each did.

```c
static inline uint32_t rdcycle(void) {
    uint32_t c; __asm__ volatile ("rdcycle %0" : "=r"(c)); return c;
}
```

**Sink.** The numbers need somewhere to go. The sim console register (T5) is the cleanest:
`put_str("fill "); put_hex(t1 - t0);`. Alternative without T5: write them to a fixed table
in data RAM and have `tb_soc_top` read the Verilator model's memory array after `finished`.

**Hardware-side counter too.** Add `BUSY_CYCLES` (R) to `dotp_ctrl`, counting cycles between
start and done. Firmware-side timing of the accelerator is polluted by poll-loop granularity
(a load plus a branch is ~8-12 cycles at 4 CPI), so the hardware number is the honest one.

**Why first.** Every other item is a claim until this exists. Ten lines of C.

## 2. PicoRV32 parameter tuning (T1)

PicoRV32 is a multi-cycle, non-pipelined core: one instruction at a time, each taking several
states, with every instruction fetch itself a bus transaction. Default configuration is
~4 CPI. Parameters that matter here:

| Parameter | Set to | Effect |
|---|---|---|
| `ENABLE_MUL` | 1 | `mul` via the PCPI sequential multiplier: ~32 cycles per multiply. Still 5x better than libgcc `__mulsi3` under rv32i. |
| `ENABLE_FAST_MUL` | 1 | Replaces it with `picorv32_pcpi_fast_mul`, which uses `*` (one DSP): a few cycles per multiply. 64 multiplies per reference go from ~2,000 cycles to ~250. |
| `BARREL_SHIFTER` | 1 | Default shifter moves 1 bit per cycle (4 then 1 with `TWO_STAGE_SHIFT`). xorshift32 does shifts of 13, 17, 5 per call, and GCC emits `slli`/`srai` 16 pairs for every int16 truncation. Barrel makes each single-cycle. Bigger than it looks. |
| `ENABLE_REGS_DUALPORT` | 1 (default) | Both operands read in one cycle. Keep. |
| `TWO_CYCLE_ALU`, `TWO_CYCLE_COMPARE` | 0 (default) | Fewer cycles per instruction. These are the knobs to flip to 1 later if the core limits Fmax on the board. |
| `ENABLE_COUNTERS` | 1 | For T6. |
| `CATCH_MISALIGN`, `CATCH_ILLINSN` | 1 (default) | Core asserts `trap` on a bug. Wire `trap` out of `soc_top` and fail the testbench the moment it rises. Free assertion. |
| `ENABLE_REGS_16_31` | 1 (default) | Required for full RV32I; GCC uses those registers. |

Firmware side: `-march=rv32im -mabi=ilp32 -O2`, and confirm the disassembly contains `mul`.

**Invariant check.** `ENABLE_FAST_MUL` puts a multiplier in the control core. Invariant 1 is
about vector arithmetic; the spec explicitly allows the software reference. Record it in
`decisions.md` so nobody reads the DSP count and thinks the core is cheating.

**Expected.** Reference phase 5-10x faster, fill phase ~1.5x, overall 2-3x per iteration.
Cost is one DSP and a few hundred LEs on a 176K LE part.

## 3. Word-packed stores (T2)

**Today.** xorshift32 yields 32 bits, firmware splits it into two int16 and does two `sh`
stores. Each store is a full bus transaction plus the fetch of the store instruction itself,
which at 4 CPI is the expensive part.

**Change.** Store the 32-bit word once with `sw`. Element `i` lives at byte `2i`, little-endian,
so the low half is element `2k` and the high half is `2k+1`: exactly the packed layout the
scratchpad already expects. No hardware change, no layout change, same bytes.

```c
uint32_t w = xorshift32(&s);
A_words[k] = w;                         /* one sw instead of two sh */
int16_t a0 = (int16_t)(w & 0xffff);     /* same two elements as before */
int16_t a1 = (int16_t)(w >> 16);
```

**Fuse the passes.** Generate the word, store it, extract the two elements, and accumulate the
reference in the same loop. One pass over 32 words per vector, no `int16_t[64]` copy in data
RAM, no second pass of loads. Generation and reference become one function.

**Golden model unchanged.** D8 already defines the generator as "two int16 per call", so the
element sequence is identical. The C++ model does not change. Byte-strobe coverage moves to the
scratchpad module test, since firmware no longer issues halfword writes.

**Expected.** Fill phase down roughly 30-40%.

## 4. Firmware reorder (T3)

**Today.** fill, reference, start, poll. The core spins for the whole accelerator run and
observes `done` late because of poll granularity.

**Change.** fill, start, reference, then one `STATUS` read. The accelerator finishes ~20
cycles into a ~2,000-cycle reference computation, so `done` is already set. That single read is
now an assertion, not a wait: if `done` is clear after the reference, something is wrong, and
firmware sets the fail bit. Keep a bounded poll as a fallback for robustness.

**Contract to write down.** Firmware must not write the scratchpads while `busy`. The reorder
satisfies this naturally (the next fill starts only after `done` is checked), but put it in the
`dotp_ctrl` spec. Optionally, have hardware flag a scratchpad write during busy in a sticky
STATUS bit (pairs with T8).

**Why it matters later.** This loop shape is what makes ping-pong (M5) or a faster core (H1)
pay off. With the current shape the accelerator latency is always exposed; with this one it is
hidden as long as the reference takes longer than the accelerator does.

## 5. Look-ahead RAM (E1)

**The handshake.** PicoRV32 raises `mem_valid` with `mem_addr`; the slave raises `mem_ready`
when `mem_rdata` is valid; the core samples `mem_rdata` in the cycle both are high. A
synchronous RAM (address registered, data the next cycle, which is what a BRAM is) sees the
address in cycle 0 and answers in cycle 1: every access costs at least two cycles.

**The look-ahead port.** PicoRV32 also drives `mem_la_read`, `mem_la_write`, `mem_la_addr`,
`mem_la_wdata`, `mem_la_wstrb` one cycle *before* `mem_valid`, combinationally from its state.
If the RAM's address register is loaded from `mem_la_addr` whenever `mem_la_read` is high, the
BRAM output is already valid in the cycle `mem_valid` rises, and the RAM asserts `mem_ready`
in that same cycle. Zero wait states. Writes use `mem_la_write` the same way.

**Where.** `ram_instr` and `ram_data` only. Peripherals stay on the plain path; they see few
accesses. The decoder must also decode `mem_la_addr[19:16]` in parallel, which is the same
nibble compare it already does.

**Impact.** One cycle saved per memory transaction, and every instruction is at least one
transaction, so roughly 20-30% of all cycles across every firmware phase.

**Pitfalls.** `mem_ready` becomes combinational from `mem_valid`, so there is a core to decoder
to RAM to core combinational path to watch for timing on the board. Keep `LATCHED_MEM_RDATA=0`
(default): the core latches `mem_rdata` itself. The RAM module testbenches must drive the
`la` signals. Invariant 3 still holds, with a bound of zero.

## 6. DSP path registering (E3)

**Ti180 DSP block.** 19x18 signed multiplier, 48-bit adder/accumulator, with optional input,
pipeline, and output registers. Efinity infers it from `*` and absorbs adjacent flops into
those register stages, but only when the RTL flops are shaped to match: registered operands
feeding the multiply directly, a registered product directly after it, no reset on the data
registers, no logic between register and multiplier beyond a clock enable.

**Proposed lane pipeline.**

| Stage | Contents |
|---|---|
| 0 | Scratchpad row read (BRAM output register): 64-bit A row, 64-bit B row |
| 1 | Operand registers, sliced 16-bit, `$signed` |
| 2 | `prod_q <= a_q * b_q`, 32-bit signed |
| 3, 4 | Adder tree stages (4 to 2 to 1) |
| 5 | 48-bit accumulator |

A one-bit `valid` shift register runs alongside so `dotp_ctrl` knows when the last product has
landed. Drain length is a `localparam PIPE_DEPTH` in `soc_pkg.sv`, used by the drain counter
and by the testbenches, so changing the depth later touches one line.

**Why.** Unregistered, the critical path is BRAM read, 16x16 multiply, two adds, 48-bit
accumulate, all in one cycle: ~100 MHz territory. Registered, each stage is one BRAM read,
one in-DSP multiply, or one add: comfortably above 200 MHz on the Ti180.

**Two classic mistakes to avoid.** Both operands must be `$signed`, or Verilog multiplies them
unsigned and negatives come out wrong. The product must be declared 32 bits wide, or the
multiply is truncated to the operand width. Also: no synchronous reset on memory arrays, or
they become flops and muxes instead of BRAM.

**Alternative shape (M2).** `acc_p <= acc_p + a_p * b_p` per lane uses the DSP's own
accumulator, and a single 4-input reduce runs once at the end. The adder tree leaves the
per-cycle path entirely. Either shape works; pick one and record it.

## 7. The verification signals (T4, T5, T9, T10)

**T4 PARAMS register.** `0x14 PARAMS = {FIFO_DEPTH, ELEM_BITS, P, N}` as four bytes, plus
`0x18 VERSION`. Firmware at boot compares it against the same fields built from `memmap.h`
macros and fails immediately on mismatch. Invariant 2 ("package and header agree") is now
checked on every `make test` rather than trusted. One case arm in `dotp_ctrl`.

**T5 Sim console.** Region `0x0007_0000`: `0x00 PUTC` (W), `0x04 EXIT` (W, exit code). Give
`soc_top` two extra outputs, `sim_putc_valid` and `sim_putc_data`, that the testbench prints
and the board leaves unconnected (or routes to a UART later). Firmware gets a tiny `put_str`
and `put_hex32`, no printf. This is what turns T6's numbers into something you can read, and
what makes a failing `tb_soc_top` say *which* iteration and *which* value was wrong.

**T9 Assertions.** Immediate assertions inside `always_ff`, run under `verilator --assert`:

- `result_fifo`: never pop when empty, count never exceeds depth.
- `bus_decoder`: select is one-hot or zero.
- `dotp_ctrl`: row index below N/P, no start accepted while busy, no scratchpad bus write
  while busy.
- `soc_top`: PicoRV32 `trap` never rises.

Guard with `ifndef SYNTHESIS`. Payoff: failures localise to a cycle and a module instead of a
mismatch at the end of a 100k-cycle run.

**T10 Coverage.** `verilator --coverage` plus a `make cov` target and `verilator_coverage
--annotate` for hit counts on source. Add `cover` points for LENGTH<N, LENGTH==N, FIFO full,
FIFO overflow, overflow W1C, and each decoder region. Success criterion 3 in the project
overview becomes a number in the README instead of a sentence.

## 8. Throughput mode (M1, with M2 and M4)

**Goal.** A run where the accelerator is the bottleneck, so the design has a throughput number
rather than only a correctness result.

**Registers added to `dotp_ctrl`.**

| Offset | Name | Meaning |
|---|---|---|
| `0x1C` | SEED | RW, generator seed |
| `0x20` | COUNT | RW, dot products to run back to back |
| `0x24` | BUSY_CYCLES | R, cycles from start to final done |
| `0x28` | PRODUCTS_DONE | R |
| `0x2C`, `0x30` | CHECKSUM_LO/HI | R, sum mod 2^48 of every result in the run |
| CTRL bit 2 | HW_GEN | operands come from the generator, not the scratchpads |
| CTRL bit 3 | PUSH_EN | push results to the FIFO (default 1; throughput mode clears it) |

**Generator.** To feed four lanes per cycle you need four int16 pairs per cycle: eight
xorshift32 streams (four for A, four for B), each seeded from SEED xor a lane constant, each
producing one 32-bit word per cycle, using the low half this cycle and the high half the next
(or simply two elements per stream per cycle from two streams per lane). Stream straight into
the lane operand registers through a mux; the scratchpads are bypassed in this mode. This
avoids a fill phase, a private write port, and any need for ping-pong buffers: one dot product
every 16 cycles, plus the drain bubble that M4 removes.

**Self-check.** Firmware reimplements the eight-stream generator in C, computes all COUNT
products in software (slow, but the test is not timed: 1,000 products is a few million
cycles, seconds in Verilator), and compares its running checksum to CHECKSUM. The C++
testbench does the same independently. FIFO pushes are disabled for the run so COUNT can exceed
16 without tripping overflow.

**The number.** `BUSY_CYCLES / COUNT` is cycles per product, reported through the console.
With N=64 and P=4 that is 16 cycles per product, 4 MAC per cycle: at 200 MHz, 800 M MAC/s and
12.5 M dot products/s. At P=8 (M6) it doubles.

**Invariants.** Invariant 1 holds (the core does no vector arithmetic; the software checksum is
the reference). Invariant 4 holds. D8 ("no hardware RNG in v1") gets a superseding entry in
`decisions.md`, which D8 itself anticipated.

**Cost.** A generator module with its own testbench, the lane-input mux, six registers, the
checksum adder, a firmware mode, and the doc updates. A day or two. It is the largest item in
the set and the one that changes the story from "it works" to "it works and here is how fast".
