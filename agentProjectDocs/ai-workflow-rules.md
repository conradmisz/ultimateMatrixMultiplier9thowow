# AI Workflow Rules

<!-- Keep under ~100 lines when filled. -->

## Approach

Build this SoC bottom-up and spec-driven. The context files and
`docs/research/2026-09-18-architecture-research.md` define what to build; decisions D1 to D11
there are fixed. Each hardware module is implemented together with its C++ testbench and golden
model, and is not done until the test passes with two seeds and lint is clean. Integration
(`soc_top` plus firmware) comes last, after every leaf module is verified. Do not infer or invent
register behaviour; if it is not in `architecture.md`, add it there first.

## Feature Specs

- Before implementing a non-trivial unit (any RTL module with an FSM or bus interface, the
  firmware, the top-level testbench), copy `specs/feature-template.md` to `specs/<unit>.md` and
  fill it out.
- Acceptance criteria must be written and open questions resolved before RTL is written.
- Trivial units (`gpio`, `mac_lane`, the Makefile) may skip a spec but still need a testbench.

## Scoping Rules

- One module plus its testbench per unit of work.
- Prefer small, verifiable increments: a module, its test, its PASS line, then the next.
- Do not combine RTL changes and firmware changes in one step unless the unit is `soc_top`.

## When to Split Work

Split an implementation step if it combines:

- Two RTL modules that could be tested independently.
- A change to `soc_pkg.sv` / `memmap.h` and a consumer of the changed constant (change the
  constants first, run all tests, then the consumer).
- Firmware logic and testbench harness logic.
- Behaviour not clearly defined in `architecture.md`.

If a change cannot be verified with one `make test-<name>` run, the scope is too broad.

## Handling Missing Requirements

- Do not invent bus, register, or FSM behaviour not defined in `architecture.md`.
- If a requirement is ambiguous, resolve it in `architecture.md` (and `decisions.md` if it is a
  choice) before implementing.
- If a requirement is missing, add it as an open question in `progress-tracker.md` and stop.

## Protected Files

Do not modify the following unless explicitly instructed:

- `rtl/vendor/picorv32.v` — third-party core, verbatim.
- `docs/research/2026-09-18-architecture-research.md` — historical record; append, never rewrite.
- `agentProjectDocs/specs/feature-template.md`.

## Keeping Docs in Sync

Update the relevant context file whenever implementation changes:

- Memory map, register bits, or parameters (`architecture.md`).
- Module list or boundaries (`architecture.md`, `project-overview.md`).
- Coding conventions discovered the hard way (`code-standards.md`).
- Any choice with a rejected alternative (`decisions.md`).

## Before Moving to the Next Unit

1. The unit's testbench passes with both Makefile seeds; PASS lines pasted in the report.
2. Its spec's acceptance criteria pass (if it has a spec).
3. `verilator --lint-only -Wall` is clean for the new module.
4. `make test` still passes for every previously completed unit.
5. `progress-tracker.md` reflects the completed work.
6. Any lasting technical decision made during the unit is recorded in `decisions.md`.
