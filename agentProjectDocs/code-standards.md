# Code Standards

<!-- Keep under ~120 lines when filled. -->

## General

- One module per file, file name equals module name.
- Keep modules single-purpose; if a module grows past ~150 lines, split it.
- Fix root causes. No `// TODO` or `// hack` left in committed code.
- Parameters and addresses come from `soc_pkg.sv` / `memmap.h`, never literal numbers in modules.

## SystemVerilog

- `always_ff @(posedge clk)` for state, `always_comb` for logic. Continuous `assign` is allowed
  for simple wiring. No plain `always` blocks.
- Synchronous active-low reset `rst_n` in every sequential block. No asynchronous resets.
- `logic` everywhere; no `reg`/`wire` in new code. Signed arithmetic uses `logic signed`.
- Enumerated types for FSM states, defined in the module or `soc_pkg.sv`.
- Every module has a parameter list even if it only forwards package defaults.
- No `initial` blocks except `$readmemh` in `ram32` when `INIT_FROM_PLUSARG` is set. No `#`
  delays in RTL.
- Widths explicit on every port; no implicit truncation without a comment naming the intent.
- Memories are inferred `logic [W-1:0] mem [DEPTH]` arrays; multiplies are `*`. No vendor
  primitives, no `(* ram_style *)` attributes in this phase.
- Must pass `make lint`, which runs Verilator with `-Wall -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM
  -Wno-PINCONNECTEMPTY` (the project lint policy, see decisions.md). Vendor warnings are
  silenced only through `rtl/vendor/picorv32.vlt`; no per-file or per-line suppression on our
  RTL.

## C++ Testbenches

- C++17, one `tb_<module>.cpp` per module, built with `verilator --cc --exe --build --trace-fst`.
- Use the `tb/common/` helper for clock, reset, eval, and trace; do not reimplement the loop.
- Every test takes `--seed <n>` and `--iters <n>`; the default seed is fixed and the Makefile
  also runs a second seed.
- Golden models are plain C++ functions in `tb/common/`, shared across tests (dot product,
  xorshift32, FIFO model). The xorshift implementation must be bit-identical to the firmware's.
- Checks use a small `CHECK(cond, msg)` macro that records failures and continues; the test
  exits 1 at the end if any failed.

## Firmware C

- `-march=rv32i -mabi=ilp32 -O2 -ffreestanding -nostdlib`, no libc, no printf.
- All register access through `volatile uint32_t*` helpers in `memmap.h`; no raw casts in
  `main.c`.
- Reference results accumulate in `int64_t`; the compare masks to 48 bits.
- Only one function touches RING_LO/RING_HI.

## Testing and Verification

- Framework: Verilator C++ testbenches, run via `make test`.
- What must have tests: every module in `rtl/` except the vendor core, plus `soc_top`.
- Definition of verified: the module test passes with both Makefile seeds, `tb_soc_top` passes,
  and lint is clean. A passing build alone is not verified.
- Never mark a unit complete without pasting the `PASS` lines from the actual run.

## File Organization

- `rtl/` — SystemVerilog modules and `soc_pkg.sv`.
- `rtl/vendor/` — third-party RTL, verbatim.
- `tb/` — C++ testbenches; `tb/common/` shared helpers and golden models.
- `firmware/` — C sources, startup, linker script, `memmap.h`, its Makefile.
- `sim/` — generated only.
- `docs/` — research notes, README.

## Never Do

- Never edit `rtl/vendor/picorv32.v`.
- Never hardcode an address, width, or vector size in a module or testbench; use the package or
  header constant.
- Never change a constant in `soc_pkg.sv` without the matching change in `memmap.h`.
- Never read RING_HI in firmware or a testbench except from the single drain function.
- Never use asynchronous reset, `always @*`, or `#` delays in RTL.
- Never suppress a lint warning on a non-vendor file with a `lint_off` pragma or per-file
  `-Wno-*`; the only warnings silenced project-wide are the three in the `make lint` policy
  above, and vendor warnings go only in `rtl/vendor/picorv32.vlt`.
- Never let a testbench pass by default: a test with zero checks executed must fail.
- Never add a Python, cocotb, or other simulation dependency without an entry in decisions.md.
