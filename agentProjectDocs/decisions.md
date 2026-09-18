# Decisions Log

Append-only record of significant technical decisions.
Never rewrite or delete past entries — if a decision is
reversed, add a new entry that supersedes it. Read this
before revisiting any settled design question.

Format:

## [YYYY-MM-DD] — [Short decision title]

- **Decision:** [What was decided]
- **Why:** [The reasoning — constraints, trade-offs]
- **Alternatives rejected:** [What else was considered
  and why it lost]
- **Supersedes:** [Link to earlier entry, if any]

---

## 2026-09-18 — Computation is a repeated vector dot product

- **Decision:** Dot product of two N=64 int16 vectors yielding one 48-bit scalar, repeated on
  fresh random pairs; results collected in a ring buffer.
- **Why:** Clarified with the author; the challenge's "dot matrix multiplication" meant this.
- **Alternatives rejected:** Matrix-vector product (more hardware, not what was asked).
- **Supersedes:** —

## 2026-09-18 — PicoRV32 as the control core, native memory port

- **Decision:** PicoRV32 RV32I, unmodified, using its native valid/ready memory interface. No
  AXI adapter, no interrupts, no PCPI.
- **Why:** One Verilog file, trivially simulated, the author owns the SoC structure. The Ti180
  (176K LEs, 640 DSPs) makes fit a non-issue.
- **Alternatives rejected:** Efinix Sapphire (generated, Efinity-dependent, opaque interconnect);
  Ibex (SystemVerilog but heavy build system).
- **Supersedes:** —

## 2026-09-18 — Simple memory-mapped bus, not AXI-Lite

- **Decision:** A bus decoder on address bits [19:16] with a shared valid/ready handshake to each
  peripheral; unmapped regions return ready with zero data.
- **Why:** One master, one transaction in flight, every peripheral testable alone.
- **Alternatives rejected:** AXI4-Lite everywhere (more code and corner cases for no benefit);
  hybrid with an AXI-Lite accelerator wrapper (deferred as a stretch goal).
- **Supersedes:** —

## 2026-09-18 — Sizing N=64, P=4, int16, 48-bit accumulator

- **Decision:** Parameters N=64, P=4, ELEM_BITS=16, ROW_BITS=64, ACC_BITS=48 in `soc_pkg.sv`.
- **Why:** int16 matches the Ti180 19x18 DSP; 48 bits is the DSP accumulator width; 16 cycles per
  product makes parallelism visible while keeping firmware fill time and traces short.
- **Alternatives rejected:** N=32/P=2 (too small to be interesting); N=256/P=8 (long firmware
  fill, long traces).
- **Supersedes:** —

## 2026-09-18 — Packed halfword scratchpads with a wide private read port

- **Decision:** Scratchpads are 16 rows x 64 bits with byte-enable bus writes; firmware writes
  `int16_t[64]` arrays directly; the accelerator reads one row per cycle on a private port.
- **Why:** C halfword stores already pack two elements per word; no packing code, no read mux.
- **Alternatives rejected:** One element per 32-bit word (wastes half the storage, no simpler);
  firmware pre-packing 64-bit rows (awkward C for no gain).
- **Supersedes:** —

## 2026-09-18 — Result FIFO with pop-on-read of RING_HI

- **Decision:** 16 x 48-bit hardware FIFO. RING_COUNT and RING_LO are side effect free; reading
  RING_HI returns bits 47:32 and pops. Push when full drops the entry and sets a sticky overflow
  bit in STATUS (write-1-to-clear).
- **Why:** No pointer arithmetic in firmware; hardware owns wrap-around; simple queue model in the
  testbench.
- **Alternatives rejected:** Memory-mapped ring with visible head/tail (firmware and hardware can
  disagree); no buffer (drops a spec requirement).
- **Supersedes:** —

## 2026-09-18 — Software xorshift32 for vector generation

- **Decision:** Firmware generates vectors with xorshift32 from a constant seed and computes the
  golden reference itself. The testbench reimplements the identical generator to cross-check.
- **Why:** Reproducible, enables self-check for free, zero hardware.
- **Alternatives rejected:** Hardware LFSR fill (core cannot self-check without reading back);
  both modes (stretch goal only).
- **Supersedes:** —

## 2026-09-18 — Simulation only; GPIO register stands in for LEDs

- **Decision:** No UART, LED driver, pin mapping, or Efinity flow in this phase. Firmware reports
  via a GPIO output register (heartbeat, pass, fail, finished) observed by `tb_soc_top`.
- **Why:** Author's decision to keep v1 to RTL, firmware, and testbenches. Efinity does not run on
  macOS; board bring-up needs a VM and is a separate phase.
- **Alternatives rejected:** LEDs plus UART now (extra hardware with nothing to view it on).
- **Supersedes:** —

## 2026-09-18 — C++ Verilator testbenches instead of SystemVerilog

- **Decision:** Every testbench is C++ driving a Verilator model, with C++ golden models and
  FST tracing. RTL remains SystemVerilog.
- **Why:** Trivial golden models that share the firmware's xorshift, Verilator's native path,
  fast long runs. The challenge's SystemVerilog testbench wording was judged soft by the author.
- **Alternatives rejected:** All-SV testbenches (more boilerplate-free but Verilator timing-mode
  gaps and rewritten reference models); hybrid (two flows in the Makefile); cocotb (Python
  dependency, macOS rough edges).
- **Supersedes:** —

## 2026-09-18 — Flat project layout by concern

- **Decision:** `rtl/`, `rtl/vendor/`, `tb/`, `tb/common/`, `firmware/`, `sim/`, `docs/`, one
  top-level Makefile.
- **Why:** One job per directory; agents can be scoped to one folder each.
- **Alternatives rejected:** Per-module folders (scatters the Verilator flow); FuseSoC (overhead).
- **Supersedes:** —

## 2026-09-18 — Synchronous active-low reset `rst_n`

- **Decision:** Every module uses a synchronous, active-low `rst_n`. PicoRV32's `resetn` connects
  directly.
- **Why:** Author's choice; matches the vendor core's port with no inversion.
- **Alternatives rejected:** Active-high `rst` (recommended for testbench readability, but adds an
  inverter and a second convention); asynchronous reset (unneeded, adds synchroniser concerns).
- **Supersedes:** —

## 2026-09-18 — RAM sizes 16 KB instruction, 8 KB data

- **Decision:** `ram_instr` is 16 KB, `ram_data` is 8 KB.
- **Why:** Ample for a libc-free C loop plus 16 stored references; `$readmemh` and traces stay
  fast.
- **Alternatives rejected:** 32/16 KB (headroom not needed yet); 4/4 KB (risk of not fitting at
  low optimisation).
- **Supersedes:** —

## 2026-09-18 — Firmware runs 16 iterations at LENGTH=64 and drains at the end

- **Decision:** Firmware runs exactly 16 dot products, all LENGTH=64, storing each software
  reference in data RAM, then drains all 16 results from the FIFO and compares. This fills the
  FIFO exactly without overflowing it.
- **Why:** Author's choice; exercises a full FIFO end to end. Partial LENGTH and overflow are
  covered by the `dotp_ctrl` and `result_fifo` module tests, not by firmware.
- **Alternatives rejected:** 8 iterations with one LENGTH=32 (partial length end to end but FIFO
  never full); 20 iterations (overflow end to end but loses a result by design).
- **Supersedes:** —

## 2026-09-18 — Bus decode on address bits [19:16], 64 KB regions

- **Decision:** Seven regions at `0x0000_0000` through `0x0006_0000` as listed in
  `architecture.md`; register offsets fixed there.
- **Why:** One nibble to decode, room to grow, keeps the decoder a lookup table.
- **Alternatives rejected:** Tightly packed regions (fiddlier decode for no benefit).
- **Supersedes:** —

## 2026-09-18 — Toolchain: riscv64-elf-gcc targets rv32im; PicoRV32 ENABLE_MUL=1

- **Decision:** Firmware compiles with `-march=rv32im -mabi=ilp32`; PicoRV32 is instantiated with
  `ENABLE_MUL=1` so the software reference multiply is a native `mul`. Toolchain: riscv64-elf-gcc
  (Homebrew, gcc 16.2.0) targets rv32im — confirmed by `riscv64-elf-gcc -march=rv32im -mabi=ilp32
  -O2 -c t.c -o t.o && riscv64-elf-objdump -d t.o` emitting a `mul a0,a0,a1` instruction.
- **Why:** Plain rv32i would pull libgcc's `__mulsi3` under `-nostdlib`; the hardware multiplier is
  free on this FPGA and keeps the firmware self-contained.
- **Alternatives rejected:** rv32i + `-lgcc` (works but adds a library dependency for one routine).
- **Supersedes:** —

## 2026-09-18 — Peripheral bus timing: registered one-cycle ready

- **Decision:** Every peripheral accepts a transaction on the cycle `bus_sel && !bus_ready`, performs
  its side effect and registers `bus_rdata` on that edge, and drives `bus_ready` high for exactly one
  cycle. No combinational ready.
- **Why:** One rule for registers and memories alike; safe with PicoRV32's mem_ready sampling; makes
  the C++ `bus_read`/`bus_write` helpers trivial.
- **Alternatives rejected:** Combinational same-cycle ready (faster by one cycle, but a second timing
  convention to test).
- **Supersedes:** —

## 2026-09-18 — Lint policy: -Wall minus three style warnings, no per-file suppression

- **Decision:** All Verilator invocations use `-Wall -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM -Wno-PINCONNECTEMPTY`. Vendor warnings are silenced only through `rtl/vendor/picorv32.vlt`. No `lint_off` pragmas or `-Wno-*` flags for individual non-vendor files.
- **Why:** Every bus peripheral leaves upper address and data bits unused and the top level leaves optional outputs unconnected; those three warnings would fire on correct code. Everything else in -Wall stays on.
- **Alternatives rejected:** Per-signal `unused` idioms (noise in every module); per-file lint_off (hides real problems).
- **Supersedes:** —

## 2026-09-18 — PicoRV32 multiplier: ENABLE_MUL=1, ENABLE_FAST_MUL=0

- **Decision:** PicoRV32 keeps its slow shift-add multiplier (`ENABLE_MUL=1`, `ENABLE_FAST_MUL`
  left at its default 0); firmware is compiled rv32im.
- **Why:** Unspecified by the plan; correctness first. The resulting ~245k-cycle firmware run is
  acceptable in simulation.
- **Alternatives rejected:** `ENABLE_FAST_MUL=1` (single-cycle multiply, but more logic, and not
  needed for a sim-only phase); rv32i plus libgcc's `__mulsi3` (adds a library dependency for one
  routine, see the toolchain decision above).
- **Supersedes:** —
