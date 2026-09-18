# FPGA Dot-Product SoC: Architecture Research Notes

Date: 2026-09-18
Status: living document, updated as decisions are made during the research phase.
Purpose: capture findings and decisions before the project-kickoff skill generates the agent context files.

---

## 1. Target hardware findings (Titanium Ti180 J484 Dev Kit)

Sources: `Documentation/titanium180-ds-v3.7.pdf`, `Documentation/ti180j484-devkit-ug-v1.7.pdf`,
`Documentation/ti180j484-devkit-overview-v1.0.pdf`, `Documentation/efinity-ug-v18.3.pdf`.

### FPGA resources (Ti180)
| Resource | Count |
|---|---|
| Logic elements | 176,256 |
| XLR cells (4-input LUT + FF, fracturable / full adder) | 172,800 |
| Embedded memory | 13.11 Mbit as 1,280 blocks of 10 Kbit |
| DSP blocks | 640 |
| PLLs | 8 |

DSP block modes: Normal = one 19x18 signed/unsigned multiply with 48-bit add/sub/accumulate and
cascade in/out; Dual = 11x10 + 8x8; Quad = 7x6 + three 4x4; Float = one BFLOAT16 FMA.
Block RAM is uninitialised at power-up unless initialised in the bitstream.

Conclusion: a RISC-V soft core fits trivially. PicoRV32 is ~1.5K LUTs. The device could hold dozens.
The 19x18 multiply with 48-bit accumulator maps directly to an int16 multiply-accumulate lane.

### Board I/O relevant to us
- USB-C to FTDI FT2232H: channel 0 = UART to FPGA, channel 1 = JTAG.
- 6 user LEDs (active high, banks 4B/4C), 2 pushbuttons (active low, enable internal pull-up).
- Oscillators: 25, 33.33, 50, 74.25 MHz feeding PLL inputs.
- 2x 256 Mbit SPI NOR flash (bitstream + optional RISC-V binary), LPDDR4x (not needed for us).
- The preloaded demo uses Efinix Sapphire SoC (VexRiscv) with AXI/APB, which confirms a RISC-V control plane is the idiomatic pattern on this board.

### Programming the board
- Efinity Programmer over USB JTAG: `.bit` for volatile load, `.hex` to SPI flash for persistence (SPI Active via JTAG bridge).
- Firmware can be baked into instruction block RAM at synthesis, so no flash programming of software is needed for v1.

### Efinity software: what it is needed for and constraints
Efinity is required only for: synthesis + place-and-route to bitstream (no open source flow targets Efinix),
Interface Designer (pin assignment, PLL and GPIO configuration, a separate step from RTL),
Programmer (JTAG load), and IP Manager (only if using Sapphire; not needed with PicoRV32).

Supported OS: Windows 10+, Ubuntu 20.04+, RHEL 8.8+, all x86-64. **macOS is not supported.**
Development machine is an Apple Silicon Mac, so synthesis/programming requires a VM
(Windows 11 ARM with x86 emulation via Parallels/UTM, or an x86 Ubuntu VM under UTM) with USB
passthrough of the FTDI device. This is the biggest practical risk and gates only the board demo,
not development or verification.

Mitigation: keep RTL free of vendor primitives (plain `*`, inferred memory arrays) so nothing depends
on Efinity until the bitstream stage. Efinity supports SystemVerilog input and documents iVerilog
and ModelSim as simulators.

### Local toolchain status (checked 2026-09-18)
- Verilator 5.048: installed (supports SV testbenches via `--binary` / `--timing`).
- Not installed: RISC-V GCC, GTKWave, iVerilog, cocotb, Efinity, Docker.
- PicoRV32 source: `Documentation/PicoCore/picorv32-main.zip` (GitHub main, 267 files). Key files:
  `picorv32.v` (the core, one file), `firmware/start.S` (startup stub), `firmware/sections.lds`
  (linker script), `firmware/makehex.py` (bin -> hex for `$readmemh`), `picosoc/` (a reference SoC
  showing the native mem port wired to RAM and peripherals; useful as a wiring example, not reused).
- Homebrew offers `riscv64-elf-gcc` (need to verify it builds rv32i/ilp32 targets; xpack
  `riscv-none-elf-gcc` is the fallback) and `surfer` (waveform viewer, FST/VCD).

---

## 2. Decisions made

| # | Question | Decision | Rationale |
|---|---|---|---|
| D1 | Computation | Dot product of two N-element vectors producing one scalar, repeated on fresh random pairs, results stored in a ring buffer. | Confirmed by user. Bonus output line deferred. |
| D2 | RISC-V core | PicoRV32 (native memory interface, not the AXI adapter). | One Verilog file, simple valid/ready bus, Verilator-friendly, user owns the SoC structure. Sapphire rejected as generated/opaque and Efinity-dependent; Ibex rejected as heavy build system for no gain. |
| D3 | Bus | Simple memory-mapped bus: address decoder on the PicoRV32 mem port, shared valid/ready handshake to each peripheral. | Single core, one transaction in flight, every peripheral testable alone. AXI-Lite wrapper on the accelerator is a stretch goal (hybrid). |
| D5 | Sizing | N = 64 elements, P = 4 lanes, int16 elements, 48-bit result. 16 cycles per dot product, 4 DSP blocks, 128 bytes per scratchpad. N, P, ELEM_BITS are SV parameters mirrored in a firmware header. | Parallelism visible, firmware fill time short, waveforms readable. P=1/8 reruns give a comparison table. |
| D6 | Scratchpad layout | Packed halfwords: firmware treats each scratchpad as `int16_t[64]`; halfword stores land two elements per 32-bit word via byte strobes. Physically 16 rows x ROW_BITS (= P * ELEM_BITS = 64) so the accelerator reads one row (4 elements) per cycle. Bus writes hit half a row using byte enables selected by address bit 2. | Natural C layout, no packing code, no muxing on the accelerator read side. Name the row width `ROW_BITS`, never a literal 64, to avoid confusion with N. |
| D7 | Result ring buffer | Hardware FIFO, 16 entries x 48 bits, pop-on-read. Core sees RING_COUNT and a data pair: reading RING_LO returns bits [31:0] of the oldest entry with no side effect; reading RING_HI returns bits [47:32] **and pops**. Firmware always reads LO then HI. Push on a full FIFO drops the new entry and sets a sticky overflow bit in STATUS (write-1-to-clear). Head/tail pointers are hardware-internal. | No pointer arithmetic in firmware, hardware owns wrap-around, testbench compares against a software queue. Only one firmware function may touch the data address because reads have side effects. |
| D8 | Randomness | Software xorshift32 in firmware, constant seed, two int16 per call. Firmware computes the golden reference from the same values. No hardware RNG in v1. | Reproducible in sim and on board, enables self-check for free. Hardware LFSR fill is a possible throughput-mode stretch goal. |
| D9 | Scope: simulation only for now | No UART, no LED driver, no pin assignment, no Efinity flow in v1. Firmware signals pass/fail/iteration through a simple GPIO output register that the top-level testbench observes and that could later drive LEDs. Waveforms dumped from Verilator (FST/VCD) and viewed in a waveform viewer. Board bring-up (Efinity VM, Interface Designer, LEDs/UART) is deferred to a later phase. | User decision. Keeps v1 focused on RTL + firmware + testbenches. |
| D10 | Testbench framework | C++ testbenches driving Verilator models, one per module plus one top-level SoC test. FST trace dumping, seed and iteration count via command line, golden models in C++ (dot product, queue, memory) sharing the firmware's xorshift header. A tiny shared helper (clock/reset/eval/trace) keeps per-test boilerplate small. | User decision: SystemVerilog testbench requirement is soft. C++ gives trivial golden models, Verilator's native path, fast long runs. Trade-off accepted: Verilator-only, looks less like a hardware reviewer's idiom. RTL itself remains SystemVerilog. |
| D11 | Project layout | Flat by concern: `rtl/` (SV modules, one per file), `rtl/vendor/` (picorv32.v unmodified), `tb/` (C++ testbenches, one per module, `tb/common/` helper), `firmware/` (C, start.S, linker script, Makefile -> firmware.hex), `sim/` (Verilator outputs and traces, gitignored), `docs/`, top-level Makefile (build firmware, run any/all tests, open waves). | One job per directory; agents can be pointed at one folder each. Per-module folders and FuseSoC rejected as overhead. |
| D4 | Control-plane philosophy | Core never touches the arithmetic. Firmware fills scratchpads, kicks accelerator, polls done, reads results, and computes a software golden reference for self-check. | Satisfies the mandated RISC-V control while keeping the datapath in hardware. |

Pending: none for the simulation phase. Deferred to board phase: Efinity VM plan, Interface Designer pin
assignment, UART/LED drivers.

## 4. Module inventory (derived from decisions)
| Module | Role | Tested against |
|---|---|---|
| `soc_top` | Instantiates everything, exposes clk/rst/gpio_out | Firmware run: pass/fail GPIO bits, result FIFO contents |
| `picorv32` (vendor) | RV32I control core, native mem port | Not unit tested; covered by soc_top |
| `bus_decoder` | Upper-address-bit routing, rdata/ready mux | Address -> select table |
| `ram_instr`, `ram_data` | 32-bit RAMs, instr loaded via `$readmemh` | Memory model |
| `scratchpad` (x2) | 16 rows x ROW_BITS, byte-enable bus writes, full-row read port | Memory model with strobes; row read matches packed writes |
| `mac_lane` | int16 x int16 -> 32-bit product, registered | Multiply |
| `adder_tree` | Sums P products, pipelined | Sum |
| `dotp_ctrl` | FSM: idle -> stream N/P rows -> accumulate -> done; CTRL/STATUS/LENGTH regs; pushes result to FIFO | Behavioural dot product on random vectors, incl. LENGTH < N |
| `result_fifo` | 16 x 48 bits, pop on RING_HI read, overflow sticky | Software queue |
| `gpio` | Output register (pass/fail/heartbeat bits) | Register read-back |

Firmware: `firmware/main.c` (xorshift, fill scratchpads, golden ref, start, poll, drain FIFO, compare,
set GPIO), `start.S` + `sections.lds` adapted from PicoRV32, `memmap.h` shared constants (N, P,
ELEM_BITS, base addresses, register offsets) kept in sync with an SV package `soc_pkg.sv`.

Toolchain to install: `riscv64-elf-gcc` (verify rv32i/ilp32), `surfer`. Verilator 5.048 present.


---

## 3. Explanations worth keeping

### 3.1 Why a full RISC-V core is "excessive" but still the right call
A ~20-line FSM could sequence the whole job. The challenge mandates a RISC-V for control, so the
correct framing is control plane vs data plane: the core orchestrates, the accelerator computes.
Using software randomness gives a free self-checking design: firmware computes the expected dot
product in C and compares with the hardware result, driving pass/fail LEDs.

### 3.2 Vector compute options considered
1. Single MAC: one DSP, N cycles per dot product. Trivial.
2. P parallel lanes + pipelined adder tree + accumulator: N/P cycles. **Recommended**, parametrised
   so P=1 degenerates to option 1. Reads P elements per cycle from wide scratchpad ports.
3. Systolic / matrix arrays: only justified for matrix-vector or matrix-matrix, not a dot product.

Element width: int16 (matches 19x18 DSP). 64 x (int16 x int16) needs a 38-bit accumulator; a
48-bit result register matches the DSP accumulator exactly. N and P are SystemVerilog parameters.

### 3.3 The simple memory-mapped bus (D3) in detail
PicoRV32 native port: `mem_valid` (request), `mem_addr` (32-bit byte address), `mem_wdata`,
`mem_wstrb` (4-bit byte enable, all-zero = read), slave returns `mem_rdata` + `mem_ready`. The core
holds the request until ready, so slow peripherals simply delay ready.

A decoder module looks at the upper address bits, routes the transaction to exactly one peripheral,
and muxes that peripheral's rdata/ready back. Every peripheral sees the same interface:
select, offset, wdata, wstrb, and returns rdata + ready.

Provisional memory map (subject to change):
```
0x0000_0000  instruction RAM
0x0001_0000  data RAM (stack, heap)
0x0002_0000  scratchpad A
0x0003_0000  scratchpad B
0x0004_0000  dot-product control/status registers
0x0005_0000  result ring buffer
0x0006_0000  GPIO (LEDs, buttons)
```

The accelerator has two faces: the narrow register interface the core uses, and a wide private read
port into scratchpads A and B (P elements per cycle) that the core never sees. The core writes data
in slowly over 32 bits; the accelerator streams it out at full width.

Advantages: one handshake everywhere; every peripheral testable alone; decoder is a lookup table.
Given up vs AXI-Lite: separate read/write channels, independent address/data phases, vendor
interoperability. None matter with one core and one transaction in flight.

### 3.4 Firmware control flow
Bare-metal C, RV32I/RV32IM, compiled with RISC-V GCC, baked into instruction RAM. Peripherals are
accessed through volatile pointers at fixed addresses. No OS, no driver library.

Provisional accelerator register block:
| Offset | Register | Access | Meaning |
|---|---|---|---|
| 0x00 | CTRL | W | bit0 start, bit1 soft reset |
| 0x04 | STATUS | R | bit0 busy, bit1 done, bit2 ring not empty |
| 0x08 | LENGTH | RW | elements to process, up to N |
| 0x0C | RESULT_LO | R | low 32 bits of latest 48-bit result |
| 0x10 | RESULT_HI | R | upper 16 bits |
| 0x14 | RING_COUNT | R | entries in result ring buffer |
Ring buffer region: each read pops the oldest result (simplest for firmware).

Main loop per iteration:
1. Generate two random int16 vectors (xorshift/LFSR in software; seed constant or cycle counter).
2. Store them into scratchpads A and B (optionally two int16 per 32-bit word).
3. Compute expected result in C into a 64-bit integer (golden reference).
4. Write LENGTH, write start to CTRL.
5. Poll STATUS until done (hardware finishes in tens of cycles; interrupts unnecessary).
6. Read result (registers or ring pop), compare with reference.
7. Drive pass/fail LEDs, toggle heartbeat LED, optionally print over UART.

Runtime is dominated by feeding the scratchpads and the software reference, not the hardware dot
product. State this in the write-up: the accelerator is not the bottleneck, feeding it is.

Build details: PicoRV32 needs a startup stub (set stack pointer, jump to main) and a linker script
placing code at instruction RAM base and stack in data RAM; PicoRV32's repo has examples. GCC ELF ->
objcopy binary -> hex file loaded via initial-memory statement. The same hex feeds Verilator and
Efinity so simulation runs the exact bytes the board runs.

### 3.5 Testbench strategy
C++ testbenches under Verilator (see D10), one per module, each with a C++ golden model:
MAC lane vs multiply, adder tree vs sum, accelerator vs behavioural dot product on random inputs,
ring buffer vs queue, decoder vs expected select lines, scratchpads vs a memory model. The top-level
testbench loads real compiled firmware and checks LED/result outcomes. Most debugging time will
land in the top-level integration test.

### 3.6 Terminology (to keep the numbers straight)
- N = 64: vector length, elements per vector.
- ELEM_BITS = 16: width of each element (int16, -32768..32767). Two int16 multiply to 32 bits; summing 64 of them needs 38 bits; the 48-bit result matches the DSP accumulator width.
- P = 4: multiply-accumulate lanes, i.e. element pairs consumed per clock. Each lane is one DSP block. Cycles per dot product = N / P = 16.
- ROW_BITS = P * ELEM_BITS = 64: scratchpad row width. A coincidence with N; not the vector size. Each vector is N * ELEM_BITS = 1024 bits = 128 bytes = 32 bus words.
- "Packed" = two 16-bit elements per 32-bit bus word, which is what a C `int16_t` array already does; store-halfword instructions become 32-bit bus writes with two byte strobes set.

### 3.7 Expected time sinks, in order
1. Efinity in a VM with USB passthrough.
2. RISC-V GCC + linker script producing a bootable hex for PicoRV32.
3. Top-level integration testbench.
The accelerator itself is the easy part.
