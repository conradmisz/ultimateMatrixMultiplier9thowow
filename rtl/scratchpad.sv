module scratchpad
  import soc_pkg::*;
(
  input  logic                clk,
  input  logic                rst_n,
  // bus face (processor)
  input  logic                bus_sel,
  input  logic [15:0]         bus_addr,
  input  logic [31:0]         bus_wdata,
  input  logic [3:0]          bus_wstrb,
  output logic [31:0]         bus_rdata,
  output logic                bus_ready,
  // row face (accelerator), combinational read
  input  logic [ROW_AW-1:0]   row_addr,
  output logic [ROW_BITS-1:0] row_data
);
  localparam int BYTES_PER_ROW = ROW_BITS / 8;   // 8
  localparam int WORD_AW = $clog2(SP_BYTES / 4);  // 5

  logic [7:0] mem [SP_BYTES];
  logic accept;
  logic [WORD_AW-1:0] widx;

  assign accept = bus_sel && !bus_ready;
  assign widx   = bus_addr[WORD_AW+1:2];

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      bus_ready <= 1'b0;
      bus_rdata <= '0;
    end else begin
      bus_ready <= accept;
      if (accept) begin
        for (int k = 0; k < 4; k++) begin
          int idx;
          idx = int'(widx) * 4 + k;
          bus_rdata[8*k +: 8] <= mem[idx];
          if (bus_wstrb[k]) mem[idx] <= bus_wdata[8*k +: 8];
        end
      end
    end
  end

  always_comb begin
    for (int k = 0; k < BYTES_PER_ROW; k++)
      row_data[8*k +: 8] = mem[int'(row_addr) * BYTES_PER_ROW + k];
  end
endmodule
