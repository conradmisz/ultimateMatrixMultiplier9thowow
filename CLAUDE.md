# Project Context

Context files live in `agentProjectDocs/`. Load them
conditionally — do not read all of them for every task.

## Always Read (every session)

1. `agentProjectDocs/progress-tracker.md` — current
   phase, in-progress work, next steps
2. `agentProjectDocs/architecture.md` — **Invariants**
   section at minimum

## Read Before Specific Work

| Before...                          | Read                                    |
| ---------------------------------- | --------------------------------------- |
| Product or scope decisions         | `agentProjectDocs/project-overview.md`  |
| Testbench output or trace work     | `agentProjectDocs/ui-context.md`        |
| Writing or modifying RTL, C++, C   | `agentProjectDocs/code-standards.md`    |
| Planning or starting a unit        | `agentProjectDocs/ai-workflow-rules.md` |
| Revisiting a settled design choice | `agentProjectDocs/decisions.md`         |
| Implementing a planned unit        | Its spec in `agentProjectDocs/specs/`   |
| Understanding why the design is what it is | `docs/research/2026-09-18-architecture-research.md` |

## Commands

- Build firmware: `make firmware` (produces `firmware/firmware.hex`)
- All tests: `make test`
- Single test: `make test-<module>` (e.g. `make test-result_fifo`, `make test-soc_top`)
- Single test with options: `make test-<module> ARGS="--seed 7 --iters 200"`
- Lint: `make lint` (runs `verilator --lint-only -Wall` on `rtl/`)
- Waveforms: `make waves TEST=<module>` (opens `sim/tb_<module>.fst` in Surfer)
- Clean: `make clean` (removes `sim/` and `firmware/*.hex`, `*.elf`, `*.bin`)

Toolchain: Verilator 5.048, `riscv64-elf-gcc`, `surfer` (`brew install riscv64-elf-gcc surfer`).

## Keeping Context in Sync

- Update `agentProjectDocs/progress-tracker.md` after
  each meaningful implementation change.
- Record significant technical decisions (and why) in
  `agentProjectDocs/decisions.md` — append-only.
- If implementation changes the architecture, scope, or
  standards documented in the context files, update the
  relevant file before continuing.
- Before implementing a non-trivial unit, write a
  spec in `agentProjectDocs/specs/` using
  `feature-template.md`.
- `soc_pkg.sv` and `firmware/memmap.h` change together, always.
