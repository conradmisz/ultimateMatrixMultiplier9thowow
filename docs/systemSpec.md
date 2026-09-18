# System Specification: RISC-V Dot-Product SoC

Version 1.0, 2026-09-18. Simulation phase.
Companion documents: `docs/research/2026-09-18-architecture-research.md` (why each choice was
made) and `agentProjectDocs/architecture.md` (the exact numbers agents build against).

---

## 1. What the system does

The system computes the dot product of two vectors, over and over, using a small hardware
accelerator that is told what to do by a tiny RISC-V processor.

A dot product takes two lists of numbers of the same length, multiplies them pairwise, and adds
up all the products to get a single number. Our lists have 64 entries and each entry is a 16-bit
signed integer.

```
A = [ a0, a1, a2, ... a63 ]
B = [ b0, b1, b2, ... b63 ]

result = a0*b0 + a1*b1 + a2*b2 + ... + a63*b63
```

The processor makes up the two lists using a pseudo-random generator, hands them to the
accelerator, waits for the answer, and checks it against an answer it worked out itself in
software. If every answer matches, it raises a "pass" flag. The whole thing runs in a simulator
on a laptop for now; the design is written so it can later be loaded onto an Efinix Ti180 FPGA
board without changes.

---

## 2. The big picture

```
                              +-----------------------------------------------------+
                              |                      soc_top                        |
                              |                                                     |
   clk  ---------------------->                                                     |
   rst_n --------------------->   +-----------+     one request at a time           |
                              |   | PicoRV32  |==================+                  |
                              |   | RISC-V    |                  |                  |
                              |   | core      |<=================+                  |
                              |   +-----------+                  |                  |
                              |                                  v                  |
                              |                          +-------------+            |
                              |                          | bus_decoder |            |
                              |                          +-------------+            |
                              |      _________________________|||||||_____________  |
                              |     /        /        /        |   \        \     \ |
                              |    v        v        v         v    v        v     v|
                              | ram_instr ram_data scratch_A scratch_B dotp_ctrl result_fifo gpio
                              |                       |         |     ^  |      ^        |
                              |                       |  wide   |     |  |      |        |
                              |                       |  rows   |     |  |      |        |
                              |                       +----+----+     |  |      |        |
                              |                            v          |  |      |        |
                              |                    +----------------+ |  | push |        |
                              |                    | 4 x mac_lane   | |  |      |        |
                              |                    | + adder_tree   |-+  +------+        |
                              |                    +----------------+                    |
                              |                                                          |
   gpio_out[3:0] <------------------------------------------------------------------------+
                              +-----------------------------------------------------------+
```

Two kinds of connection appear in this picture:

- **Bus connections** (the lines fanning out from `bus_decoder`). The processor uses these for
  everything it does. It can only read and write addresses; every block below the decoder looks
  to the processor like a piece of memory.
- **Private connections** (the lines between the scratchpads, the multiplier lanes, `dotp_ctrl`,
  and `result_fifo`). The accelerator uses these to fetch data and deliver answers without
  involving the processor at all.

The processor is the only thing that starts a bus transfer, and it waits for each one to finish
before starting the next. That single rule is what lets the bus stay very simple.

---

## 3. The blocks, one at a time

### 3.1 PicoRV32, the processor

An open source 32-bit RISC-V core. We use it exactly as downloaded, with no changes. Its job is to
run a small C program (the firmware) that orchestrates everything. It does not do any of the
vector arithmetic itself, apart from computing the reference answer used for checking.

It has one memory port. Every instruction it fetches and every piece of data it loads or stores
goes out through that port as a request: "here is an address, here is optional write data, tell
me when you are done and give me the read data." It does not have a separate connection for
peripherals, interrupts, or anything else in this design.

### 3.2 bus_decoder, the traffic cop

Takes each request from the processor, looks at the address, and forwards the request to
exactly one of the seven blocks below it. When that block replies, the decoder passes the reply
back to the processor. If the address does not belong to any block, the decoder replies
immediately with zero so the processor never hangs.

The address space is divided into 64 KB regions. Only the region number matters to the decoder;
each block handles the finer addressing inside its own region.

| Address range starts at | Block          | What lives there                          |
| ----------------------- | -------------- | ----------------------------------------- |
| `0x0000_0000`           | `ram_instr`    | The firmware program (16 KB)              |
| `0x0001_0000`           | `ram_data`     | Stack and variables (8 KB)                |
| `0x0002_0000`           | scratchpad A   | Vector A (128 bytes)                      |
| `0x0003_0000`           | scratchpad B   | Vector B (128 bytes)                      |
| `0x0004_0000`           | `dotp_ctrl`    | Accelerator control and status registers  |
| `0x0005_0000`           | `result_fifo`  | The queue of finished results             |
| `0x0006_0000`           | `gpio`         | The output flags (heartbeat, pass, fail, finished) |

### 3.3 ram_instr and ram_data, ordinary memory

Two plain 32-bit memories. The instruction memory is filled with the compiled firmware when the
simulation starts, from a text file called `firmware.hex`. The data memory starts empty and holds
the firmware's stack, its variables, and the 16 reference answers it saves for checking later.
Keeping them separate means instruction fetches and data traffic never wait on each other.

### 3.4 scratchpad A and B, the vector holding areas

Each scratchpad holds one 64-element vector. It has two faces:

- **The processor's face.** From the bus it looks like 128 bytes of ordinary memory. The
  firmware writes the vector into it exactly as it would write a C array of `int16_t`, two
  elements per 32-bit word, using normal half-word stores. Nothing special happens in software.
- **The accelerator's face.** Internally the 128 bytes are arranged as 16 rows of 64 bits. Each
  row holds four consecutive elements. The accelerator reads one whole row per clock cycle
  through a private port the processor cannot see.

The same bytes, viewed two ways:

```
Processor view (bytes, two elements per 32-bit word):
  word 0: [ a1 | a0 ]   word 1: [ a3 | a2 ]   word 2: [ a5 | a4 ] ...

Accelerator view (rows of four elements):
  row 0:  [ a3 | a2 | a1 | a0 ]      <- read in cycle 0
  row 1:  [ a7 | a6 | a5 | a4 ]      <- read in cycle 1
  ...
  row 15: [ a63 | a62 | a61 | a60 ]  <- read in cycle 15
```

### 3.5 The accelerator: mac_lane, adder_tree, dotp_ctrl

This is where the arithmetic happens. It is split into three small pieces.

- **mac_lane** multiplies one element of A by one element of B and gives a 32-bit product. There
  are four of these side by side, so four multiplications happen every clock cycle. On the real
  FPGA each lane becomes one hardware DSP block.
- **adder_tree** adds the four products together into one number, in a couple of pipelined
  stages so the clock can run fast.
- **dotp_ctrl** is the controller. It owns the registers the processor talks to, steps through
  the scratchpad rows, keeps a running total in a 48-bit accumulator, and when the last row is
  done it pushes the total into the result queue and raises its "done" flag.

Because four elements are consumed per cycle and there are 64 elements, one full dot product
takes 16 cycles plus a few cycles of pipeline fill.

The registers the processor sees, as byte offsets from `0x0004_0000`:

| Offset | Name       | Direction        | Meaning                                                  |
| ------ | ---------- | ---------------- | -------------------------------------------------------- |
| `0x00` | CTRL       | write            | bit 0 = start a dot product; bit 1 = reset the accelerator |
| `0x04` | STATUS     | read             | bit 0 = busy; bit 1 = done; bit 2 = the result queue overflowed (stays set until the processor writes a 1 to clear it) |
| `0x08` | LENGTH     | read and write   | how many elements to process this time, 0 to 64          |
| `0x0C` | RESULT_LO  | read             | low 32 bits of the most recent result                    |
| `0x10` | RESULT_HI  | read             | high 16 bits of the most recent result                   |

The controller's life cycle, in words: sit idle; when start is written, clear the accumulator and
walk through rows 0, 1, 2 and so on until LENGTH elements have been consumed; wait for the last
products to drain through the adder tree; push the total into the queue; raise done; go back to
idle. Writing start while busy is ignored.

### 3.6 result_fifo, the queue of answers

A first-in, first-out queue with room for 16 results of 48 bits each. The accelerator pushes a
result into it every time a dot product finishes. The processor takes results out in the order
they went in.

Registers, as byte offsets from `0x0005_0000`:

| Offset | Name        | Direction | Meaning                                                    |
| ------ | ----------- | --------- | ---------------------------------------------------------- |
| `0x00` | RING_COUNT  | read      | how many results are waiting                               |
| `0x04` | RING_LO     | read      | low 32 bits of the oldest result; reading does nothing else |
| `0x08` | RING_HI     | read      | high 16 bits of the oldest result, **and removes it from the queue** |

That last point is the one unusual behaviour in the design. Reading RING_HI has a side effect:
it pops the queue. Firmware therefore always reads LO first, then HI, and only one function in
the firmware is allowed to touch these two addresses. If the accelerator tries to push into a
full queue, the new result is thrown away and the overflow bit in STATUS is set so the problem
is visible.

### 3.7 gpio, the output flags

A single 4-bit register the processor writes and the outside world can see. In simulation the
testbench watches it; on the board the same four bits would drive LEDs.

| Bit | Name      | Meaning                                                     |
| --- | --------- | ----------------------------------------------------------- |
| 0   | heartbeat | flips every iteration so you can see the loop is alive       |
| 1   | pass      | set when every result matched its reference                 |
| 2   | fail      | set when any result did not match                           |
| 3   | finished  | set when the firmware has completed its run                 |

### 3.8 soc_top, the container

Wires all of the above together. Its only external connections are the clock, the reset, and
the four GPIO bits. Everything else is internal.

---

## 4. One complete run, step by step

This is what happens from reset to the finished flag. The firmware does 16 iterations.

1. **Reset is released.** The processor starts executing at address 0, where the firmware lives.
2. **Firmware initialises.** Sets up its stack, seeds the pseudo-random generator with a fixed
   constant so every run is identical, and writes LENGTH = 64.
3. **For each of the 16 iterations:**
   1. Generate 64 random 16-bit values and store them into scratchpad A, then 64 more into
      scratchpad B. This is roughly 64 bus writes and takes a few hundred cycles.
   2. Multiply and add the same values in software to get the expected answer, and save it in
      data memory. This also takes a few hundred cycles.
   3. Write 1 to CTRL to start the accelerator.
   4. Read STATUS repeatedly until the done bit is set. This takes about 20 cycles.
   5. Flip the heartbeat bit.
4. **Drain the queue.** The queue now holds exactly 16 results. Firmware reads RING_COUNT to
   confirm, then for each entry reads RING_LO, then RING_HI, and compares the 48-bit value with
   the saved expected answer.
5. **Report.** If all 16 matched, set the pass bit. Otherwise set the fail bit. Then set the
   finished bit.
6. **The testbench sees the finished bit**, checks that pass is set and fail is clear, and also
   recomputes all 16 answers itself from the same seed to confirm the firmware and hardware agree
   with an independent third party. It then exits with success or failure.

A note on where the time goes: the accelerator spends about 20 cycles per dot product, but the
processor spends several hundred cycles feeding it and several hundred more computing the
reference. This is expected for a control-plane design and is worth stating plainly. The point
of the exercise is the architecture, not the throughput.

---

## 5. Sizes and numbers in one place

| Name        | Value | Meaning                                                |
| ----------- | ----- | ------------------------------------------------------ |
| N           | 64    | elements in each vector                                |
| ELEM_BITS   | 16    | width of each element (signed)                         |
| P           | 4     | multiplier lanes, elements consumed per cycle          |
| ROW_BITS    | 64    | scratchpad row width, equals P times ELEM_BITS         |
| ACC_BITS    | 48    | result and accumulator width                           |
| FIFO_DEPTH  | 16    | results the queue can hold                             |
| Instr RAM   | 16 KB | firmware program                                       |
| Data RAM    | 8 KB  | stack, variables, saved references                     |
| Iterations  | 16    | dot products per firmware run                          |

The largest possible result is two 16-bit numbers multiplied (32 bits) summed 64 times (6 more
bits), so 38 bits. The 48-bit width is chosen because that is the natural accumulator width of
the FPGA's DSP block, so it costs nothing extra.

All of these live in one SystemVerilog package and one C header that must always agree.

---

## 6. Design rules that must never be broken

1. The processor never does vector arithmetic. All multiply-and-add happens in the accelerator.
2. The SystemVerilog package and the C header define the same sizes and addresses.
3. Only one bus request is ever in flight, and every address answers, including unmapped ones.
4. Reading RING_HI is the only bus action that pops the queue.
5. No FPGA-vendor-specific building blocks in the RTL. Multiplies are written as `*` and
   memories as plain arrays so the design simulates anywhere and synthesises later.
6. Reset is synchronous and active-low on every block.
7. The PicoRV32 source file is never edited.

---

## 7. How it is verified

Every block has its own C++ testbench that runs under Verilator and compares the block against a
simple software model:

| Block          | Compared against                                                       |
| -------------- | ---------------------------------------------------------------------- |
| bus_decoder    | a table of addresses and which select line should fire                 |
| ram_instr, ram_data | a software memory, including partial-word writes                  |
| scratchpad     | a software memory; rows read back must match packed writes             |
| mac_lane       | a plain multiply                                                        |
| adder_tree     | a plain sum                                                             |
| dotp_ctrl      | a software dot product on random vectors, including LENGTH less than 64 |
| result_fifo    | a software queue, including the full and overflow cases                |
| gpio           | write then read back                                                    |
| soc_top        | runs the real firmware; pass flag set, fail clear, queue contents match |

Each test takes a random seed from the command line and is run with two different seeds. Every
test writes a waveform file that can be opened in a viewer when something goes wrong.

---

## 8. What is deliberately left out of this phase

- Anything to do with the physical board: pin assignments, clocks, LEDs, the serial port, the
  Efinix toolchain. The design is ready for it but this phase stops at simulation.
- Interrupts, custom processor instructions, and standard buses such as AXI. None are needed for
  one processor talking to a handful of registers.
- Hardware random number generation, floating point, matrix operations, and any attempt to make
  the accelerator faster than four lanes.
