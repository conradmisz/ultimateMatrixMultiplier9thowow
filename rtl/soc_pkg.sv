package soc_pkg;
  // Vector / accelerator sizing (decision D5)
  localparam int N          = 64;
  localparam int P          = 4;
  localparam int ELEM_BITS  = 16;
  localparam int ROW_BITS   = P * ELEM_BITS;     // 64
  localparam int NUM_ROWS   = N / P;             // 16
  localparam int ROW_AW     = $clog2(NUM_ROWS);  // 4
  localparam int PROD_BITS  = 2 * ELEM_BITS;     // 32
  localparam int SUM_BITS   = PROD_BITS + $clog2(P); // 34
  localparam int ACC_BITS   = 48;
  localparam int LEN_BITS   = $clog2(N + 1);     // 7
  localparam int FIFO_DEPTH = 16;

  // Memories
  localparam int INSTR_RAM_BYTES = 16384;
  localparam int DATA_RAM_BYTES  = 8192;
  localparam int SP_BYTES        = N * ELEM_BITS / 8; // 128

  // Memory map (decision: 64 KB regions, decode on addr[19:16])
  localparam int REGION_LSB   = 16;
  localparam int REGION_BITS  = 4;
  localparam int NUM_REGIONS  = 7;
  localparam int R_INSTR = 0, R_DATA = 1, R_SPA = 2, R_SPB = 3,
                 R_DOTP  = 4, R_FIFO = 5, R_GPIO = 6;
  localparam logic [31:0] BASE_INSTR = 32'h0000_0000;
  localparam logic [31:0] BASE_DATA  = 32'h0001_0000;
  localparam logic [31:0] BASE_SPA   = 32'h0002_0000;
  localparam logic [31:0] BASE_SPB   = 32'h0003_0000;
  localparam logic [31:0] BASE_DOTP  = 32'h0004_0000;
  localparam logic [31:0] BASE_FIFO  = 32'h0005_0000;
  localparam logic [31:0] BASE_GPIO  = 32'h0006_0000;
  localparam logic [31:0] STACK_TOP  = BASE_DATA + DATA_RAM_BYTES; // 0x0001_2000

  // dotp_ctrl register offsets and bits
  localparam logic [15:0] DOTP_CTRL      = 16'h00;
  localparam logic [15:0] DOTP_STATUS    = 16'h04;
  localparam logic [15:0] DOTP_LENGTH    = 16'h08;
  localparam logic [15:0] DOTP_RESULT_LO = 16'h0C;
  localparam logic [15:0] DOTP_RESULT_HI = 16'h10;
  localparam int CTRL_START = 0, CTRL_RESET = 1;
  localparam int STATUS_BUSY = 0, STATUS_DONE = 1, STATUS_OVERFLOW = 2;

  // result_fifo register offsets
  localparam logic [15:0] FIFO_COUNT = 16'h00;
  localparam logic [15:0] FIFO_LO    = 16'h04;
  localparam logic [15:0] FIFO_HI    = 16'h08;

  // gpio
  localparam int GPIO_W = 4;
  localparam logic [15:0] GPIO_OUT = 16'h00;
  localparam int GPIO_HEARTBEAT = 0, GPIO_PASS = 1, GPIO_FAIL = 2, GPIO_FINISHED = 3;
endpackage
