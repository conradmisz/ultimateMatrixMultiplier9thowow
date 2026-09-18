VERILATOR ?= verilator
CXX       ?= c++
SEEDS     ?= 1 2
ARGS      ?=

LINTFLAGS = -Wall -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM -Wno-PINCONNECTEMPTY

VFLAGS = $(LINTFLAGS) --cc --exe --build -j 0 --trace-fst --x-assign unique --x-initial unique \
         -Irtl -Itb/common -Ifirmware -CFLAGS "-std=c++17 -I$(CURDIR)/tb/common -I$(CURDIR)/firmware -O1"

PKG      = rtl/soc_pkg.sv
VENDOR   = rtl/vendor/picorv32.vlt rtl/vendor/picorv32.v

SRCS_gpio        = $(PKG) rtl/gpio.sv
SRCS_mac_lane    = $(PKG) rtl/mac_lane.sv
SRCS_adder_tree  = $(PKG) rtl/adder_tree.sv
SRCS_scratchpad  = $(PKG) rtl/scratchpad.sv
SRCS_result_fifo = $(PKG) rtl/result_fifo.sv
SRCS_bus_decoder = $(PKG) rtl/bus_decoder.sv
SRCS_ram32       = $(PKG) rtl/ram32.sv
SRCS_dotp_ctrl   = $(PKG) rtl/mac_lane.sv rtl/adder_tree.sv rtl/dotp_ctrl.sv
SRCS_soc_top     = $(PKG) rtl/gpio.sv rtl/mac_lane.sv rtl/adder_tree.sv rtl/scratchpad.sv \
                   rtl/result_fifo.sv rtl/bus_decoder.sv rtl/ram32.sv rtl/dotp_ctrl.sv \
                   rtl/soc_top.sv $(VENDOR)

TESTS = gpio mac_lane adder_tree scratchpad result_fifo bus_decoder ram32 dotp_ctrl soc_top

VFLAGS_ram32 = -GBYTES=4096 -GINIT_FROM_PLUSARG=1
ARGS_ram32   = +hex=tb/data/ram_test.hex
ARGS_soc_top = +hex=firmware/firmware.hex

.PHONY: all lint firmware test test-models clean waves
.SECONDEXPANSION:
# Keep sim/tb_<name> binaries around after each run (not deleted as chained-rule
# intermediates) so `make waves` can still find them.
.SECONDARY:

all: test

lint: $(PKG)
	$(VERILATOR) --lint-only $(LINTFLAGS) -Irtl $(PKG) $(filter-out $(PKG),$(wildcard rtl/*.sv)) $(VENDOR) --top-module soc_top

firmware:
	$(MAKE) -C firmware

sim/tb_%: $$(SRCS_$$*) tb/tb_%.cpp tb/common/harness.h tb/common/bus.h tb/common/models.h firmware/dotp_ref.h firmware/memmap.h
	@mkdir -p sim
	$(VERILATOR) $(VFLAGS) $(VFLAGS_$*) --top-module $* --Mdir sim/obj_$* -o ../tb_$* $(SRCS_$*) tb/tb_$*.cpp

test-soc_top: firmware
test-%: sim/tb_%
	@for s in $(SEEDS); do sim/tb_$* --seed $$s $(ARGS_$*) $(ARGS) || exit 1; done

sim/test_models: tb/test_models.cpp tb/common/models.h firmware/dotp_ref.h
	@mkdir -p sim
	$(CXX) -std=c++17 -Itb/common -Ifirmware -o $@ tb/test_models.cpp

test-models: sim/test_models
	sim/test_models

test: test-models $(addprefix test-,$(TESTS))

waves:
	surfer sim/tb_$(TEST).fst

clean:
	rm -rf sim
	$(MAKE) -C firmware clean
