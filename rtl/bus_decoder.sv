module bus_decoder
  import soc_pkg::*;
(
  input  logic                    clk,
  input  logic                    rst_n,
  // PicoRV32 native memory port
  input  logic                    mem_valid,
  input  logic [31:0]             mem_addr,
  input  logic [31:0]             mem_wdata,
  input  logic [3:0]              mem_wstrb,
  output logic [31:0]             mem_rdata,
  output logic                    mem_ready,
  // shared peripheral side
  output logic [NUM_REGIONS-1:0]  sel,
  output logic [15:0]             p_addr,
  output logic [31:0]             p_wdata,
  output logic [3:0]              p_wstrb,
  input  logic [31:0]             p_rdata [NUM_REGIONS],
  input  logic [NUM_REGIONS-1:0]  p_ready
);
  localparam int SEL_AW = $clog2(NUM_REGIONS);

  logic [REGION_BITS-1:0] region;
  logic mapped, unmapped_ready;

  assign region = mem_addr[REGION_LSB +: REGION_BITS];
  assign mapped = (mem_addr[31:REGION_LSB+REGION_BITS] == '0) && (region < NUM_REGIONS[REGION_BITS-1:0]);

  // region < NUM_REGIONS (<=7) whenever mapped, so the low SEL_AW bits alone index sel safely.
  always_comb begin
    sel = '0;
    if (mem_valid && mapped) sel[region[SEL_AW-1:0]] = 1'b1;
  end

  assign p_addr  = mem_addr[15:0];
  assign p_wdata = mem_wdata;
  assign p_wstrb = mem_wstrb;

  always_ff @(posedge clk) begin
    if (!rst_n) unmapped_ready <= 1'b0;
    else        unmapped_ready <= mem_valid && !mapped && !unmapped_ready;
  end

  always_comb begin
    mem_ready = unmapped_ready;
    mem_rdata = '0;
    for (int i = 0; i < NUM_REGIONS; i++) begin
      if (sel[i] && p_ready[i]) begin
        mem_ready = 1'b1;
        mem_rdata = p_rdata[i];
      end
    end
  end
endmodule
