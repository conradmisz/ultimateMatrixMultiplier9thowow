# RISC-V Dot-Product SoC (Ti180 challenge)

<!-- Keep under ~100 lines when filled. -->

## Overview

A small system-on-chip, written in SystemVerilog and verified in simulation with Verilator, that
performs repeated dot products of two randomised 64-element int16 vectors. An open source PicoRV32
RISC-V core acts purely as the control plane: firmware generates the vectors, writes them into
scratchpad memories, starts a hardware dot-product accelerator, and checks the hardware result
against a software reference. The eventual target is the Efinix Titanium Ti180 J484 development
kit; this phase is simulation only. Audience: an interview panel evaluating FPGA architecture
judgement, plus the author as the person who will later bring it up on the board.

## Goals

1. Demonstrate a clean control-plane / data-plane split: the core never performs the arithmetic.
2. Every module has its own testbench with a golden model; the whole SoC runs real compiled
   firmware to a self-checked pass.
3. RTL is vendor-primitive free so the same sources synthesise in Efinity without edits later.

## Core User Flow

1. Developer runs `make firmware` to compile `firmware/main.c` into `firmware.hex`.
2. Developer runs `make test` which builds every Verilator testbench and runs them all.
3. `tb_soc_top` loads the hex into instruction RAM, releases reset, and runs the core.
4. Firmware loops 16 times: generate A and B with xorshift32, store them to the scratchpads,
   compute the reference, program LENGTH, start the accelerator, poll for done, and store the
   reference in data RAM.
5. After the loop, firmware drains the 16 results from the FIFO, compares each, and sets the
   GPIO pass or fail bit, then the finished bit.
6. The testbench sees the finished bit, checks pass set and fail clear, and exits 0.
7. Developer opens any trace with `make waves TEST=<name>` in Surfer when something fails.

## Features

### Hardware (rtl/)

- PicoRV32 RV32I core on its native memory port, unmodified vendor source.
- Bus decoder: single-transaction valid/ready routing by address bits [19:16].
- 16 KB instruction RAM (hex-initialised) and 8 KB data RAM, both with byte strobes.
- Two scratchpads, 64 x int16 each, 16 rows x 64 bits, byte-enable bus writes, private
  full-row read port for the accelerator.
- Dot-product accelerator: 4 MAC lanes, pipelined adder tree, 48-bit accumulator, FSM with
  CTRL / STATUS / LENGTH registers, 16 cycles per 64-element dot product.
- Result FIFO: 16 x 48 bits, pop on RING_HI read, sticky overflow flag.
- GPIO output register: heartbeat, pass, fail, finished bits.

### Firmware (firmware/)

- Bare-metal C, startup stub and linker script adapted from PicoRV32, memory map header shared
  in spirit with the SystemVerilog package.

### Verification (tb/)

- C++ Verilator testbench per module with a C++ golden model and command-line seed.
- Top-level testbench running the real firmware hex to completion with FST tracing.

## Scope

### In Scope

- Everything listed under Features, the top-level Makefile, and a root `README.md` describing
  the architecture, memory map, and how to run tests.
- Verilator lint clean RTL.

### Out of Scope

- Board bring-up: Efinity project, Interface Designer pin assignment, PLL setup, LED and
  pushbutton wiring, UART, SPI flash programming.
- Interrupts, PCPI custom instructions, AXI or any standard bus wrapper.
- Hardware random number generation, matrix-vector or matrix-matrix products, floating point.
- Any throughput optimisation beyond the fixed P = 4 lanes.
- SystemVerilog testbenches (requirement waived, see decisions.md).

## Success Criteria

1. `make test` from a clean checkout builds firmware and every testbench and exits 0.
2. `tb_soc_top` finishes with GPIO pass = 1, fail = 0, finished = 1, and the 16 FIFO results
   equal the firmware's references and an independent C++ recomputation from the same seed.
3. Every module testbench passes with at least two different seeds, and `dotp_ctrl` is covered for
   LENGTH < N as well as LENGTH = N.
4. `verilator --lint-only` on `rtl/` reports no warnings for non-vendor files.
