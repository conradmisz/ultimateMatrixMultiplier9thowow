# Architecture Context

<!-- Keep under ~120 lines when filled. -->

Source of truth for the reasoning behind every choice here:
`docs/research/2026-09-18-architecture-research.md` (decisions D1 to D11).

## Stack

| Layer        | Technology                         | Role                                          |
| ------------ | ---------------------------------- | --------------------------------------------- |
| RTL          | SystemVerilog (IEEE 1800-2017)     | All hardware except the vendor core           |
| Control core | PicoRV32 (Verilog, `rtl/vendor/`)  | RV32I control plane, native mem port          |
| Simulation   | Verilator 5.048, C++17 testbenches | Module and SoC verification, FST traces       |
| Firmware     | C, riscv64-elf-gcc (rv32im, ilp32) | Control program baked into instruction RAM; `ENABLE_MUL=1` |
| Waveforms    | Surfer                             | Trace viewing                                 |
| Build        | GNU Make                           | Firmware, testbench build/run, waves          |
| Target (later) | Efinix Ti180 J484, Efinity       | Out of scope this phase                       |

## System Boundaries

- `rtl/` — every SystemVerilog module, one per file, plus `soc_pkg.sv` holding parameters and
  the memory map. Owns all hardware behaviour. `ram32` is the one shared RAM module,
  instantiated twice in `soc_top` (instruction RAM with `INIT_FROM_PLUSARG` set, data RAM
  without).
- `rtl/vendor/` — `picorv32.v` verbatim. Never edited.
- `tb/` — one C++ testbench per module plus `tb_soc_top.cpp`; `tb/common/` owns the shared
  clock/reset/eval/trace helper and golden-model utilities (xorshift, dot product, queue).
- `firmware/` — `main.c`, `start.S`, `sections.lds`, `memmap.h`, Makefile producing
  `firmware.hex`. Owns all software behaviour.
- `sim/` — generated Verilator objects and traces. Gitignored, never hand-edited.
- `docs/` — research notes, README, later specs.

## Storage Model

- **Instruction RAM** (16 KB, `ram32` instance `u_instr`): firmware code and read-only data,
  loaded from `firmware.hex` with `$readmemh` at simulation start (`INIT_FROM_PLUSARG`).
  Read-only at runtime by convention.
- **Data RAM** (8 KB, `ram32` instance `u_data`): stack, globals, and the 16 stored reference
  results.
- **Scratchpad A / B** (128 bytes each): the current input vectors. Written by the core, read
  by the accelerator. Contents are overwritten every iteration.
- **Result FIFO** (16 x 48 bits): completed dot products until the core drains them.
- **Registers**: CTRL, STATUS, LENGTH, RESULT_LO/HI, RING_COUNT/LO/HI, GPIO.

## Memory Map and Registers

Decoder selects on address bits [19:16]. Each region is 64 KB; unused space within a region is
not decoded further. Unmapped regions return ready with rdata 0 (no bus hang).

| Base          | Region        | Registers (byte offset)                                        |
| ------------- | ------------- | -------------------------------------------------------------- |
| `0x0000_0000` | ram_instr     | 16 KB, word addressed                                          |
| `0x0001_0000` | ram_data      | 8 KB, word addressed                                           |
| `0x0002_0000` | scratchpad A  | 128 bytes, `int16_t[64]` layout                                |
| `0x0003_0000` | scratchpad B  | 128 bytes, `int16_t[64]` layout                                |
| `0x0004_0000` | dotp_ctrl     | 0x00 CTRL (W: bit0 start, bit1 soft reset), 0x04 STATUS (R: bit0 busy, bit1 done, bit2 fifo overflow sticky W1C), 0x08 LENGTH (RW, 1..64), 0x0C RESULT_LO (R), 0x10 RESULT_HI (R, bits 47:32) |
| `0x0005_0000` | result_fifo   | 0x00 RING_COUNT (R), 0x04 RING_LO (R, no side effect), 0x08 RING_HI (R, pops) |
| `0x0006_0000` | gpio          | 0x00 OUT (RW: bit0 heartbeat, bit1 pass, bit2 fail, bit3 finished) |

Scratchpad element `i` lives at byte offset `2*i`. Row `r` (64 bits, elements 4r..4r+3) is
written as two 32-bit bus words at offsets `8r` (low half, address bit 2 = 0) and `8r+4`.

## Core Entities

| Entity        | Key Fields                                         | Relationships                          |
| ------------- | -------------------------------------------------- | -------------------------------------- |
| Vector        | 64 x int16 (N, ELEM_BITS)                          | Two per dot product, one per scratchpad |
| Row           | 4 x int16 = 64 bits (P, ROW_BITS)                  | 16 rows per vector, one per cycle      |
| Result        | 48-bit signed accumulator value                    | One per dot product, pushed to FIFO    |
| Bus transaction | valid, addr[31:0], wdata, wstrb[3:0], rdata, ready | One in flight at a time, from PicoRV32 |
| Parameters    | N=64, P=4, ELEM_BITS=16, ROW_BITS=64, ACC_BITS=48, FIFO_DEPTH=16 | Defined once in `soc_pkg.sv`, mirrored in `memmap.h` |

## Auth and Access Model

Not applicable. Single bus master (the core); the accelerator has private ports to the
scratchpads and FIFO that are not bus-accessible.

## Top-Level Ports

`soc_top` exposes `clk`, `rst_n`, `gpio_out` (the software-visible heartbeat/pass/fail/finished
bits), plus three debug-only outputs with no bus-visible equivalent: `trap` (PicoRV32's trap
output, wired straight through — asserted means the CPU halted on an illegal instruction or
misaligned access, which the system test checks stays low), and `dbg_result_valid` /
`dbg_result` (combinationally mirror the accelerator's FIFO-push handshake, `push_valid` /
`push_data`, for waveform inspection without draining the FIFO).

## Environment and Setup

- Tools: Verilator 5.048 (present), `brew install riscv64-elf-gcc surfer`. Homebrew
  `riscv64-elf-gcc` 16.2 is verified to target rv32im/ilp32.
- No env vars, no services, no secrets. `make firmware` produces the only generated input.
- Host: macOS Apple Silicon. Efinity does not run here; board phase needs a Linux/Windows VM.

## Invariants

1. The PicoRV32 core never performs vector arithmetic; all multiply-accumulate happens in
   `mac_lane` / `adder_tree` / `dotp_ctrl`.
2. `soc_pkg.sv` and `firmware/memmap.h` define the same N, P, ELEM_BITS, base addresses and
   register offsets. A change to one is a change to both in the same commit.
3. Exactly one bus transaction is in flight at any time; every decoded region asserts ready
   within a bounded number of cycles, and unmapped addresses still return ready.
4. Reading RING_HI is the only bus action that pops the FIFO; RING_LO and RING_COUNT are side
   effect free. Firmware reads LO before HI.
5. RTL contains no Efinix primitives, no `initial` blocks other than `$readmemh` for
   instruction RAM, and no latches. Multiplies are `*`, memories are inferred arrays.
6. Reset is synchronous, active-low `rst_n`, on every module. PicoRV32's `resetn` connects
   directly.
7. `rtl/vendor/picorv32.v` is never modified; PicoRV32 is configured only through parameters at
   instantiation.
8. Swapping scratchpad A and B in the wiring would be undetectable by `tb_soc_top`, because the
   dot product is commutative; that wiring is verified by inspection of `soc_top.sv`, not by the
   system test.
