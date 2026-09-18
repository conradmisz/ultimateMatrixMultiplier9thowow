# Progress Tracker

Update this file after every meaningful implementation
change. This file holds *ephemeral state only* — record
lasting technical decisions in `decisions.md` instead.

Pruning rule: when Completed exceeds ~10 items, collapse
older entries to one summary line each. Keep this file
under ~60 lines.

## Current Phase

- Simulation phase complete.

## Current Goal

- None; awaiting the board phase (Efinity, board bring-up).

## Completed

- Task 1: toolchain, repo skeleton, `soc_pkg.sv`, `rtl/vendor/picorv32.{v,vlt}`, shared headers, test harness.
- Task 2: `gpio` module and testbench.
- Task 3: `mac_lane` module and testbench.
- Task 4: `adder_tree` module and testbench.
- Task 5: `scratchpad` module and testbench.
- Task 6: `result_fifo` module and testbench.
- Task 7: `bus_decoder` module and testbench.
- Task 8: `ram32` module and testbench.
- Task 9: `dotp_ctrl` module and testbench.
- Task 10: `soc_top` integration and testbench.
- Task 11: firmware (`main.c`, `start.S`, `sections.lds`, `memmap.h`) and full-system verification (244,876 cycles).
- Task 12: README, agent context sync, clean-checkout verification.

## In Progress

- None.

## Next Up

- Board phase: Efinity VM, Interface Designer, LED/UART.

## Open Questions

- None; resolved items are recorded in `decisions.md`.

## Session Notes

- Research prompt used for kickoff: `docs/research/kickoff-prompt.md`.
- Host is macOS Apple Silicon; Efinity is a later, VM-based phase.
- Deferred minor review findings are listed in the SDD ledger and were triaged in the final
  whole-branch review.

## Deferred from the final review (board phase or later)

- result_fifo: a push while full is dropped even if a pop retires an entry on the same cycle (unreachable in this SoC).
- dotp_ctrl: a soft reset landing on the final DRAIN cycle still pushes (push_valid is combinational); accepted race.
- scratchpad: `int'()` index arithmetic and a bare `int` local; prefer packed index expressions before synthesis. The combinational 8-byte row read will infer distributed RAM, deliberately (128 B).
- adder_tree is a linear accumulate chain the synthesizer will balance; name promises a tree.
- bus_decoder: `NUM_REGIONS[REGION_BITS-1:0]` is correct up to 15 regions only.
- tb_ram32: `WORDS = 1024` must match `-GBYTES=4096` by hand; unused `<cstring>` include.
- bus.h: timeout bound 16 is a bare literal in two places.
- gpio.sv: literal 32 in the zero-extension (no bus-width package constant exists).
