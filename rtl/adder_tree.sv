module adder_tree
  import soc_pkg::*;
(
  input  logic                        clk,
  input  logic                        rst_n,
  input  logic signed [PROD_BITS-1:0] in [P],
  output logic signed [SUM_BITS-1:0]  sum
);
  logic signed [SUM_BITS-1:0] total;

  always_comb begin
    total = '0;
    for (int i = 0; i < P; i++) total = total + SUM_BITS'(in[i]);
  end

  always_ff @(posedge clk) begin
    if (!rst_n) sum <= '0;
    else        sum <= total;
  end
endmodule
