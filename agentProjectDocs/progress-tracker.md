# Progress Tracker

Update this file after every meaningful implementation
change. This file holds *ephemeral state only* — record
lasting technical decisions in `decisions.md` instead.

Pruning rule: when Completed exceeds ~10 items, collapse
older entries to one summary line each. Keep this file
under ~60 lines.

## Current Phase

- Not started (context files written, no RTL yet).

## Current Goal

- Produce the build plan, then implement leaf modules bottom-up with their testbenches.

## Completed

- Architecture research and decisions D1–D11 (`docs/research/2026-09-18-architecture-research.md`).
- Kickoff context files (`agentProjectDocs/`, `CLAUDE.md`).

## In Progress

- None yet.

## Next Up

- Build plan (writing-plans skill) covering, in order: toolchain install and verification,
  project skeleton and Makefile, `soc_pkg.sv` + `memmap.h`, then modules bottom-up:
  `gpio`, `mac_lane`, `adder_tree`, `scratchpad`, `result_fifo`, `bus_decoder`, `ram_instr` /
  `ram_data`, `dotp_ctrl`, `soc_top`, firmware, `tb_soc_top`, README.
- First implementation unit after the plan: toolchain verification (does Homebrew
  `riscv64-elf-gcc` emit rv32i/ilp32?) and the project skeleton.

## Open Questions

- Does Homebrew `riscv64-elf-gcc` build `-march=rv32i -mabi=ilp32` targets, or is the xpack
  `riscv-none-elf-gcc` needed? Resolve in the first unit and record in `decisions.md`.
- PicoRV32 instantiation parameters and ISA. The firmware's software reference multiplies
  int16 values; with plain rv32i GCC emits a call to libgcc's `__mulsi3`, which needs `-lgcc` under
  `-nostdlib`. Proposed: `ENABLE_MUL=1` and `-march=rv32im` so the reference is a native `mul`, plus
  `ENABLE_COUNTERS=1`, `PROGADDR_RESET=0x0`, stack at top of data RAM. Confirm during the `soc_top`
  spec and record in `decisions.md`.
- Does `dotp_ctrl` push to the FIFO on every done, or only when LENGTH > 0? Proposed: every done.

## Session Notes

- Research prompt used for kickoff: `docs/research/kickoff-prompt.md`.
- Host is macOS Apple Silicon; Efinity is a later, VM-based phase.
