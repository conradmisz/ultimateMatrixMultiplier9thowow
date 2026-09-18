# UI Context

<!-- Keep under ~100 lines when filled. -->

This project has no graphical user interface. The "UI" is the terminal output of the
testbenches and the waveform viewer. The conventions below exist so that output and traces are
consistent across every test.

## Testbench Console Output

- Every testbench prints one line per check only on failure, and a final line
  `PASS <tb_name> seed=<seed> cycles=<n>` or `FAIL <tb_name> ...` and exits 0 or 1.
- No decorative banners, no progress spinners. `make test` output should be one line per test.
- Print values in hex for addresses and bus data, decimal for element values and results.

## Waveform Conventions

- Traces are FST, written to `sim/<tb_name>.fst`, opened with `make waves TEST=<tb_name>`.
- Tracing is on by default in module tests and in `tb_soc_top`; a `--no-trace` flag disables it
  for long runs.
- Signal naming to keep traces readable: `clk`, `rst_n`, bus signals prefixed `bus_`
  (`bus_valid`, `bus_addr`, `bus_wdata`, `bus_wstrb`, `bus_rdata`, `bus_ready`), per-peripheral
  select lines `sel_<name>`, accelerator FSM state `state` as an enum so Surfer shows names.

## Theme, Colors, Typography, Border Radius, Component Library, Layout Patterns, Icons

Not applicable.
