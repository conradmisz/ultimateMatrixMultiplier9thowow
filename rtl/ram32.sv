module ram32
  import soc_pkg::*;
#(
  parameter int BYTES = 4096,
  parameter bit INIT_FROM_PLUSARG = 1'b0
) (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        bus_sel,
  input  logic [15:0] bus_addr,
  input  logic [31:0] bus_wdata,
  input  logic [3:0]  bus_wstrb,
  output logic [31:0] bus_rdata,
  output logic        bus_ready
);
  localparam int WORDS = BYTES / 4;
  localparam int AW    = $clog2(WORDS);

  logic [31:0] mem [WORDS];
  logic accept;
  logic [AW-1:0] widx;

  assign accept = bus_sel && !bus_ready;
  assign widx   = bus_addr[AW+1:2];

  // Simulation-only preload of the firmware image (decision D9: no other initial blocks in RTL).
  if (INIT_FROM_PLUSARG) begin : g_init
    string hexfile;
    initial begin
      if ($value$plusargs("hex=%s", hexfile)) $readmemh(hexfile, mem);
    end
  end

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      bus_ready <= 1'b0;
      bus_rdata <= '0;
    end else begin
      bus_ready <= accept;
      if (accept) begin
        bus_rdata <= mem[widx];
        for (int k = 0; k < 4; k++)
          if (bus_wstrb[k]) mem[widx][8*k +: 8] <= bus_wdata[8*k +: 8];
      end
    end
  end
endmodule
