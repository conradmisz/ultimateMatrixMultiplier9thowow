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
