I am building a small RISC-V controlled dot-product accelerator SoC in SystemVerilog, verified in
simulation with Verilator. All architecture research and decisions are already made and recorded in
`docs/research/2026-09-18-architecture-research.md`. Read that file first; treat its decisions
D1 through D11 as fixed, and treat the hardware findings, terminology, and module inventory there as
the source of truth. Do not reopen decisions unless you find a contradiction, in which case ask.

## What is to be built

A simulation-only SoC with this structure:

1. **Control core**: PicoRV32 (RV32I, native memory port, no AXI adapter, no interrupts, no PCPI),
   taken unmodified from `Documentation/PicoCore/picorv32-main.zip` and placed in `rtl/vendor/`.
2. **Bus decoder** (`bus_decoder`): routes the single PicoRV32 memory transaction by upper address
   bits to exactly one peripheral using a shared valid/ready handshake; muxes rdata/ready back.
3. **Memories**: `ram_instr` (loaded from `firmware.hex` via `$readmemh`) and `ram_data`
   (stack/heap), both 32-bit with byte strobes.
4. **Two scratchpads** (`scratchpad` x2, A and B): each 64 x int16, physically 16 rows x ROW_BITS
   (= P * ELEM_BITS = 64). Bus port: 32-bit writes with byte enables, address bit 2 selects the
   half-row. Private read port: one full row per cycle for the accelerator.
5. **Dot-product accelerator**: `mac_lane` (int16 x int16 -> 32-bit, registered), `adder_tree`
   (sums P = 4 products, pipelined), `dotp_ctrl` (FSM idle -> stream N/P rows -> accumulate ->
   done; 48-bit accumulator; CTRL/STATUS/LENGTH registers; pushes each result to the FIFO).
   N = 64, P = 4, ELEM_BITS = 16 as SystemVerilog parameters.
6. **Result FIFO** (`result_fifo`): 16 entries x 48 bits, pop-on-read. RING_COUNT, RING_LO (no side
   effect), RING_HI (returns upper bits and pops). Push when full drops the entry and sets a sticky
   overflow bit in STATUS (write-1-to-clear).
7. **GPIO** (`gpio`): output register carrying pass, fail, and heartbeat bits, observed by the
   top-level testbench. Stands in for the board LEDs.
8. **Top level** (`soc_top`): instantiates all of the above; ports are clk, rst, gpio_out.
9. **Shared constants**: `rtl/soc_pkg.sv` and `firmware/memmap.h` carry the same N, P, ELEM_BITS,
   base addresses, and register offsets. They must not drift.
10. **Firmware** (`firmware/`): bare-metal C `main.c` plus `start.S` and `sections.lds` adapted from
    the PicoRV32 repo, and a Makefile producing `firmware.hex`. Per iteration: xorshift32 with a
    constant seed generates A and B as `int16_t[64]` arrays written straight into the scratchpads,
    computes the golden reference in C (64-bit accumulate), sets LENGTH, writes start, polls STATUS
    until done, drains the FIFO (LO then HI), compares, sets GPIO pass/fail and toggles heartbeat.
    Runs a fixed number of iterations then sets a "finished" bit.
11. **Testbenches** (`tb/`): C++ under Verilator, one per module listed in the research notes'
    module inventory, plus `tb/tb_soc_top.cpp` which loads the real `firmware.hex`, runs until the
    finished bit, and checks pass/fail. A `tb/common/` helper handles clock, reset, eval, and FST
    tracing. Every test has a C++ golden model (multiply, sum, behavioural dot product on random
    vectors including LENGTH < N, software queue, memory model with strobes, address -> select
    table). Seeds and iteration counts come from the command line.
12. **Build**: a top-level Makefile that builds the firmware, builds and runs any single test or all
    tests, and opens a chosen FST trace in Surfer. `sim/` holds build outputs and is gitignored.

## Project layout (fixed, decision D11)
rtl/, rtl/vendor/, tb/, tb/common/, firmware/, sim/, docs/, top-level Makefile.

## Constraints
- RTL in SystemVerilog, vendor-primitive free (plain `*`, inferred memory arrays) so it will later
  synthesise in Efinity without changes.
- Testbenches in C++ (decision D10). The SystemVerilog-testbench requirement in the original
  challenge is soft and has been waived.
- Simulation only. No UART, no LED pin mapping, no Efinity project, no board bring-up in this phase.
- Toolchain: Verilator 5.048 (installed), `riscv64-elf-gcc` and `surfer` via Homebrew (to install;
  verify the GCC targets rv32i / ilp32 before relying on it).
- macOS Apple Silicon host.

## Definition of done for this phase
- Every module testbench passes under Verilator with a non-trivial random seed.
- `tb_soc_top` runs the compiled firmware to completion with the pass bit set and the fail bit clear,
  and the FIFO contents match the firmware's reference values.
- `make test` runs everything from a clean checkout; `make waves TEST=<name>` opens a trace.
- A short README in `docs/` explaining the architecture, the memory map, and how to run the tests.

Please run the project-kickoff skill using this prompt and the research notes as the project brief,
then we will produce a build plan.
