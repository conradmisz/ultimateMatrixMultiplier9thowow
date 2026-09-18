#ifndef MEMMAP_H
#define MEMMAP_H
#include <stdint.h>

#define N_ELEMS      64
#define P_LANES      4
#define ELEM_BITS    16
#define ACC_BITS     48
#define FIFO_DEPTH   16
#define INSTR_RAM_BYTES 16384
#define DATA_RAM_BYTES  8192

#define BASE_INSTR 0x00000000u
#define BASE_DATA  0x00010000u
#define BASE_SPA   0x00020000u
#define BASE_SPB   0x00030000u
#define BASE_DOTP  0x00040000u
#define BASE_FIFO  0x00050000u
#define BASE_GPIO  0x00060000u

#define DOTP_CTRL      0x00u
#define DOTP_STATUS    0x04u
#define DOTP_LENGTH    0x08u
#define DOTP_RESULT_LO 0x0Cu
#define DOTP_RESULT_HI 0x10u
#define CTRL_START_BIT   (1u << 0)
#define CTRL_RESET_BIT   (1u << 1)
#define STATUS_BUSY_BIT  (1u << 0)
#define STATUS_DONE_BIT  (1u << 1)
#define STATUS_OVF_BIT   (1u << 2)

#define FIFO_COUNT 0x00u
#define FIFO_LO    0x04u
#define FIFO_HI    0x08u

#define GPIO_OUT   0x00u
#define GPIO_HEARTBEAT_BIT (1u << 0)
#define GPIO_PASS_BIT      (1u << 1)
#define GPIO_FAIL_BIT      (1u << 2)
#define GPIO_FINISHED_BIT  (1u << 3)

#define ACC_MASK 0x0000FFFFFFFFFFFFull

#define REG32(addr) (*(volatile uint32_t *)(uintptr_t)(addr))

#endif
