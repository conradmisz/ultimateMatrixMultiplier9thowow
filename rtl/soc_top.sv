module soc_top
  import soc_pkg::*;
(
  input  logic                clk,
  input  logic                rst_n,
  output logic [GPIO_W-1:0]   gpio_out,
  output logic                trap,
  output logic                dbg_result_valid,
  output logic [ACC_BITS-1:0] dbg_result
);
  // ---- core memory port
  logic        mem_valid, mem_ready;
  logic [31:0] mem_addr, mem_wdata, mem_rdata;
  logic [3:0]  mem_wstrb;

  // ---- decoded peripheral bus
  logic [NUM_REGIONS-1:0] sel, p_ready;
  logic [15:0] p_addr;
  logic [31:0] p_wdata;
  logic [3:0]  p_wstrb;
  logic [31:0] p_rdata [NUM_REGIONS];

  // ---- private accelerator wiring
  logic [ROW_AW-1:0]   row_addr;
  logic [ROW_BITS-1:0] row_a, row_b;
  logic                push_valid, fifo_overflow;
  logic [ACC_BITS-1:0] push_data;

  assign dbg_result_valid = push_valid;
  assign dbg_result       = push_data;

  picorv32 #(
    .ENABLE_COUNTERS (1),
    .ENABLE_COUNTERS64 (1),
    .ENABLE_REGS_16_31 (1),
    .ENABLE_REGS_DUALPORT (1),
    .BARREL_SHIFTER (1),
    .COMPRESSED_ISA (0),
    .ENABLE_MUL (1),
    .ENABLE_DIV (0),
    .ENABLE_IRQ (0),
    .ENABLE_PCPI (0),
    .CATCH_MISALIGN (1),
    .CATCH_ILLINSN (1),
    .PROGADDR_RESET (BASE_INSTR),
    .STACKADDR (STACK_TOP)
  ) u_cpu (
    .clk (clk), .resetn (rst_n), .trap (trap),
    .mem_valid (mem_valid), .mem_instr (), .mem_ready (mem_ready),
    .mem_addr (mem_addr), .mem_wdata (mem_wdata), .mem_wstrb (mem_wstrb), .mem_rdata (mem_rdata),
    .mem_la_read (), .mem_la_write (), .mem_la_addr (), .mem_la_wdata (), .mem_la_wstrb (),
    .pcpi_valid (), .pcpi_insn (), .pcpi_rs1 (), .pcpi_rs2 (),
    .pcpi_wr (1'b0), .pcpi_rd (32'd0), .pcpi_wait (1'b0), .pcpi_ready (1'b0),
    .irq (32'd0), .eoi (),
    .trace_valid (), .trace_data ()
  );

  bus_decoder u_dec (
    .clk (clk), .rst_n (rst_n),
    .mem_valid (mem_valid), .mem_addr (mem_addr), .mem_wdata (mem_wdata), .mem_wstrb (mem_wstrb),
    .mem_rdata (mem_rdata), .mem_ready (mem_ready),
    .sel (sel), .p_addr (p_addr), .p_wdata (p_wdata), .p_wstrb (p_wstrb),
    .p_rdata (p_rdata), .p_ready (p_ready)
  );

  ram32 #(.BYTES (INSTR_RAM_BYTES), .INIT_FROM_PLUSARG (1'b1)) u_instr (
    .clk (clk), .rst_n (rst_n), .bus_sel (sel[R_INSTR]), .bus_addr (p_addr),
    .bus_wdata (p_wdata), .bus_wstrb (p_wstrb), .bus_rdata (p_rdata[R_INSTR]), .bus_ready (p_ready[R_INSTR]));

  ram32 #(.BYTES (DATA_RAM_BYTES)) u_data (
    .clk (clk), .rst_n (rst_n), .bus_sel (sel[R_DATA]), .bus_addr (p_addr),
    .bus_wdata (p_wdata), .bus_wstrb (p_wstrb), .bus_rdata (p_rdata[R_DATA]), .bus_ready (p_ready[R_DATA]));

  scratchpad u_spa (
    .clk (clk), .rst_n (rst_n), .bus_sel (sel[R_SPA]), .bus_addr (p_addr),
    .bus_wdata (p_wdata), .bus_wstrb (p_wstrb), .bus_rdata (p_rdata[R_SPA]), .bus_ready (p_ready[R_SPA]),
    .row_addr (row_addr), .row_data (row_a));

  scratchpad u_spb (
    .clk (clk), .rst_n (rst_n), .bus_sel (sel[R_SPB]), .bus_addr (p_addr),
    .bus_wdata (p_wdata), .bus_wstrb (p_wstrb), .bus_rdata (p_rdata[R_SPB]), .bus_ready (p_ready[R_SPB]),
    .row_addr (row_addr), .row_data (row_b));

  dotp_ctrl u_dotp (
    .clk (clk), .rst_n (rst_n), .bus_sel (sel[R_DOTP]), .bus_addr (p_addr),
    .bus_wdata (p_wdata), .bus_wstrb (p_wstrb), .bus_rdata (p_rdata[R_DOTP]), .bus_ready (p_ready[R_DOTP]),
    .row_addr (row_addr), .row_a (row_a), .row_b (row_b),
    .push_valid (push_valid), .push_data (push_data), .fifo_overflow (fifo_overflow),
    .busy (), .done ());

  result_fifo u_fifo (
    .clk (clk), .rst_n (rst_n), .bus_sel (sel[R_FIFO]), .bus_addr (p_addr),
    .bus_wdata (p_wdata), .bus_wstrb (p_wstrb), .bus_rdata (p_rdata[R_FIFO]), .bus_ready (p_ready[R_FIFO]),
    .push_valid (push_valid), .push_data (push_data), .overflow (fifo_overflow), .count ());

  gpio u_gpio (
    .clk (clk), .rst_n (rst_n), .bus_sel (sel[R_GPIO]), .bus_addr (p_addr),
    .bus_wdata (p_wdata), .bus_wstrb (p_wstrb), .bus_rdata (p_rdata[R_GPIO]), .bus_ready (p_ready[R_GPIO]),
    .gpio_out (gpio_out));

endmodule
