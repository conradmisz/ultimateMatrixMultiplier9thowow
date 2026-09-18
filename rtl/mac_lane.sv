module mac_lane
  import soc_pkg::*;
(
  input  logic                         clk,
  input  logic                         rst_n,
  input  logic signed [ELEM_BITS-1:0]  a,
  input  logic signed [ELEM_BITS-1:0]  b,
  output logic signed [PROD_BITS-1:0]  p
);
  always_ff @(posedge clk) begin
    if (!rst_n) p <= '0;
    else        p <= a * b;
  end
endmodule
