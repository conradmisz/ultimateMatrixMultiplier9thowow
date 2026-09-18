# RISC-V Dot-Product SoC

A PicoRV32 control core drives a 4-lane int16 dot-product accelerator over a simple
memory-mapped bus. Verified in Verilator; targets the Efinix Ti180 J484 (board phase pending).

Read `docs/systemSpec.md` for the plain-English architecture and
`agentProjectDocs/architecture.md` for the exact memory map and register bits.

## Layout
- `rtl/` SystemVerilog modules (`soc_pkg.sv` holds every parameter and address)
- `rtl/vendor/` PicoRV32, unmodified
- `tb/` C++ Verilator testbenches, one per module; `tb/common/` shared harness and models
- `firmware/` bare-metal C control program, built to `firmware.hex`
- `docs/` this file, the spec, research notes, plans

## Prerequisites (macOS)
    brew install verilator riscv64-elf-gcc surfer

## Run
    make test                          # firmware + every testbench, seeds 1 and 2
    make test-dotp_ctrl                # one module
    make test-dotp_ctrl ARGS="--seed 7 --iters 500"
    make test-<module> ARGS="--no-trace"  # run without writing a trace
    make lint                          # verilator --lint-only -Wall
    make waves TEST=soc_top            # open sim/tb_soc_top.fst in Surfer
    make clean

## Memory map
| Base          | Block        |
|---------------|--------------|
| `0x0000_0000` | instruction RAM (16 KB) |
| `0x0001_0000` | data RAM (8 KB) |
| `0x0002_0000` | scratchpad A |
| `0x0003_0000` | scratchpad B |
| `0x0004_0000` | dot-product control/status |
| `0x0005_0000` | result FIFO |
| `0x0006_0000` | GPIO |

## What the system test proves
`tb_soc_top` loads the compiled firmware, runs until the firmware raises the finished bit,
and checks: pass set, fail clear, 16 heartbeat toggles, no CPU trap, and all 16 hardware
results equal to an independent C++ recomputation from the same seed.

## Performance note
The firmware run takes about 245k cycles for 16 iterations, roughly 15k cycles per iteration.
That cost is dominated by PicoRV32 itself: around 5 cycles per instruction on this design's
2-cycle bus, plus the shift-add multiplier PicoRV32 uses to compute the software reference
dot product. The accelerator side is cheap by comparison: a hardware dot product takes about
20 cycles. If a faster software reference is ever wanted, `ENABLE_FAST_MUL=1` on the PicoRV32
instantiation in `rtl/soc_top.sv` is the lever (single-cycle multiply at the cost of more
logic); see `agentProjectDocs/decisions.md` for why it is currently left at 0.
