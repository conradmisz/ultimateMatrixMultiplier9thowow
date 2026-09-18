module gpio
  import soc_pkg::*;
#(
  parameter int W = GPIO_W
) (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        bus_sel,
  input  logic [15:0] bus_addr,
  input  logic [31:0] bus_wdata,
  input  logic [3:0]  bus_wstrb,
  output logic [31:0] bus_rdata,
  output logic        bus_ready,
  output logic [W-1:0] gpio_out
);
  logic accept;
  assign accept = bus_sel && !bus_ready;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      bus_ready <= 1'b0;
      bus_rdata <= '0;
      gpio_out  <= '0;
    end else begin
      bus_ready <= accept;
      if (accept) begin
        bus_rdata <= '0;
        if (bus_addr == GPIO_OUT) begin
          bus_rdata <= {{(32-W){1'b0}}, gpio_out};
          if (bus_wstrb[0]) gpio_out <= bus_wdata[W-1:0];
        end
      end
    end
  end
endmodule
