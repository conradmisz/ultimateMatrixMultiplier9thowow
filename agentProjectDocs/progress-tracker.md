# Progress Tracker

Update this file after every meaningful implementation
change. This file holds *ephemeral state only* — record
lasting technical decisions in `decisions.md` instead.

Pruning rule: when Completed exceeds ~10 items, collapse
older entries to one summary line each. Keep this file
under ~60 lines.

## Current Phase

- In progress (Task 1 done: toolchain, skeleton, soc_pkg, shared headers, test harness).

## Current Goal

- Produce the build plan, then implement leaf modules bottom-up with their testbenches.

## Completed

- Architecture research and decisions D1–D11 (`docs/research/2026-09-18-architecture-research.md`).
- Kickoff context files (`agentProjectDocs/`, `CLAUDE.md`).
- Task 1: toolchain verified (riscv64-elf-gcc builds rv32im), repo skeleton, `rtl/soc_pkg.sv`,
  `rtl/vendor/picorv32.{v,vlt}`, `firmware/memmap.h`, `firmware/dotp_ref.h`, `firmware/makehex.py`,
  `tb/common/{harness,bus,models}.h`, `tb/test_models.cpp`, top-level and firmware `Makefile`s.

## In Progress

- None yet.

## Next Up

- Task 2: gpio.

## Open Questions

- Does `dotp_ctrl` push to the FIFO on every done, or only when LENGTH > 0? Proposed: every done.

## Session Notes

- Research prompt used for kickoff: `docs/research/kickoff-prompt.md`.
- Host is macOS Apple Silicon; Efinity is a later, VM-based phase.
