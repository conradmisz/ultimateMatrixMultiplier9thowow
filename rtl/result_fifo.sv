module result_fifo
  import soc_pkg::*;
(
  input  logic                        clk,
  input  logic                        rst_n,
  // bus face
  input  logic                        bus_sel,
  input  logic [15:0]                 bus_addr,
  input  logic [31:0]                 bus_wdata,
  input  logic [3:0]                  bus_wstrb,
  output logic [31:0]                 bus_rdata,
  output logic                        bus_ready,
  // accelerator face
  input  logic                        push_valid,
  input  logic [ACC_BITS-1:0]         push_data,
  output logic                        overflow,
  output logic [$clog2(FIFO_DEPTH):0] count
);
  localparam int AW = $clog2(FIFO_DEPTH);

  logic [ACC_BITS-1:0] mem [FIFO_DEPTH];
  logic [AW:0] head, tail;     // extra bit distinguishes full from empty
  logic accept, full, empty, do_pop, do_push;

  assign count  = head - tail;
  assign full   = (count == FIFO_DEPTH[AW:0]);
  assign empty  = (head == tail);
  assign accept = bus_sel && !bus_ready;
  assign do_pop  = accept && (bus_addr == FIFO_HI) && (bus_wstrb == 4'b0) && !empty;
  assign do_push = push_valid && !full;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      head <= '0; tail <= '0;
      bus_ready <= 1'b0; bus_rdata <= '0; overflow <= 1'b0;
    end else begin
      overflow  <= push_valid && full;
      bus_ready <= accept;
      if (do_push) begin
        mem[head[AW-1:0]] <= push_data;
        head <= head + 1'b1;
      end
      if (accept) begin
        bus_rdata <= '0;
        case (bus_addr)
          FIFO_COUNT: bus_rdata <= {{(32-AW-1){1'b0}}, count};
          FIFO_LO:    bus_rdata <= empty ? 32'd0 : mem[tail[AW-1:0]][31:0];
          FIFO_HI:    bus_rdata <= empty ? 32'd0 : 32'(mem[tail[AW-1:0]][ACC_BITS-1:32]);
          default:    bus_rdata <= '0;
        endcase
      end
      if (do_pop) tail <= tail + 1'b1;
    end
  end
endmodule
