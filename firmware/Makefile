CROSS   ?= riscv64-elf-
CC       = $(CROSS)gcc
OBJCOPY  = $(CROSS)objcopy
OBJDUMP  = $(CROSS)objdump
CFLAGS   = -march=rv32im -mabi=ilp32 -O2 -Wall -Wextra -ffreestanding -nostdlib -nostartfiles \
           -fno-builtin -fno-common -I.
LDFLAGS  = -T sections.lds -Wl,--gc-sections -Wl,-Map=firmware.map
WORDS    = 4096
# 16 KB instruction RAM

all: firmware.hex

firmware.elf: start.S main.c memmap.h dotp_ref.h sections.lds
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ start.S main.c

firmware.bin: firmware.elf
	$(OBJCOPY) -O binary --only-section=.text --only-section=.rodata --only-section=.data $< $@

firmware.hex: firmware.bin makehex.py
	python3 makehex.py $< $(WORDS) > $@

disasm: firmware.elf
	$(OBJDUMP) -d $< | less

clean:
	rm -f *.o *.elf *.bin *.hex *.map

.PHONY: all clean disasm
