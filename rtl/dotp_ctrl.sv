module dotp_ctrl
  import soc_pkg::*;
(
  input  logic                clk,
  input  logic                rst_n,
  // bus face
  input  logic                bus_sel,
  input  logic [15:0]         bus_addr,
  input  logic [31:0]         bus_wdata,
  input  logic [3:0]          bus_wstrb,
  output logic [31:0]         bus_rdata,
  output logic                bus_ready,
  // scratchpad row ports (private)
  output logic [ROW_AW-1:0]   row_addr,
  input  logic [ROW_BITS-1:0] row_a,
  input  logic [ROW_BITS-1:0] row_b,
  // result fifo (private)
  output logic                push_valid,
  output logic [ACC_BITS-1:0] push_data,
  input  logic                fifo_overflow,
  // status mirrors
  output logic                busy,
  output logic                done
);
  typedef enum logic [1:0] {IDLE, STREAM, DRAIN} state_t;
  state_t state;

  logic accept, wr, start_req, reset_req;
  logic [LEN_BITS-1:0] length_q, remaining;
  logic [ROW_AW-1:0]   row_idx;
  logic [1:0]          drain_cnt;
  logic                ovf_sticky;
  logic signed [ACC_BITS-1:0] acc, result;

  // --- pipeline: masked inputs -> P registered products -> registered sum -> accumulate
  logic signed [ELEM_BITS-1:0] a_m [P];
  logic signed [ELEM_BITS-1:0] b_m [P];
  logic signed [PROD_BITS-1:0] prod [P];
  logic signed [SUM_BITS-1:0]  sum;
  logic v1, v2;   // v1: prod valid; v2: sum valid

  assign accept    = bus_sel && !bus_ready;
  assign wr        = accept && (bus_wstrb != 4'b0);
  assign start_req = wr && (bus_addr == DOTP_CTRL) && bus_wdata[CTRL_START];
  assign reset_req = wr && (bus_addr == DOTP_CTRL) && bus_wdata[CTRL_RESET];
  assign busy      = (state != IDLE);
  assign row_addr  = row_idx;

  always_comb begin
    for (int i = 0; i < P; i++) begin
      // lane i of the current row is live if the FSM is streaming and i < remaining
      if (state == STREAM && LEN_BITS'(i) < remaining) begin
        a_m[i] = row_a[i*ELEM_BITS +: ELEM_BITS];
        b_m[i] = row_b[i*ELEM_BITS +: ELEM_BITS];
      end else begin
        a_m[i] = '0;
        b_m[i] = '0;
      end
    end
  end

  for (genvar i = 0; i < P; i++) begin : g_lane
    mac_lane u_lane (.clk(clk), .rst_n(rst_n), .a(a_m[i]), .b(b_m[i]), .p(prod[i]));
  end
  adder_tree u_tree (.clk(clk), .rst_n(rst_n), .in(prod), .sum(sum));

  // push_valid is a single-cycle pulse by construction (drain_cnt counts down once per DRAIN
  // pass and the FSM leaves DRAIN the same cycle it fires); result_fifo's overflow detection
  // relies on that.
  assign push_valid = (state == DRAIN) && (drain_cnt == 2'd0);
  assign push_data  = acc;

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      state <= IDLE; length_q <= '0; remaining <= '0; row_idx <= '0; drain_cnt <= '0;
      acc <= '0; result <= '0; done <= 1'b0; ovf_sticky <= 1'b0; v1 <= 1'b0; v2 <= 1'b0;
      bus_ready <= 1'b0; bus_rdata <= '0;
    end else begin
      bus_ready <= accept;
      v1 <= (state == STREAM);
      v2 <= v1;
      if (v2) acc <= acc + ACC_BITS'(sum);

      // register interface
      if (accept) begin
        bus_rdata <= '0;
        case (bus_addr)
          DOTP_STATUS:    bus_rdata <= {29'd0, ovf_sticky, done, busy};
          DOTP_LENGTH:    bus_rdata <= {{(32-LEN_BITS){1'b0}}, length_q};
          DOTP_RESULT_LO: bus_rdata <= result[31:0];
          DOTP_RESULT_HI: bus_rdata <= 32'(result[ACC_BITS-1:32]);
          default:        bus_rdata <= '0;
        endcase
        if (wr && bus_addr == DOTP_LENGTH)
          length_q <= ((|bus_wdata[31:LEN_BITS]) || (bus_wdata[LEN_BITS-1:0] > LEN_BITS'(N)))
                      ? LEN_BITS'(N) : bus_wdata[LEN_BITS-1:0];
        // W1C clear is applied first; a coincident fifo_overflow pulse (assigned below,
        // after this block) wins the same-cycle race so a genuine overflow is never lost.
        if (wr && bus_addr == DOTP_STATUS && bus_wdata[STATUS_OVERFLOW]) ovf_sticky <= 1'b0;
      end
      if (fifo_overflow) ovf_sticky <= 1'b1;

      // FSM
      if (reset_req) begin
        state <= IDLE; acc <= '0; done <= 1'b0; ovf_sticky <= 1'b0; row_idx <= '0;
        v1 <= 1'b0; v2 <= 1'b0; remaining <= '0; drain_cnt <= '0;
      end else begin
        case (state)
          IDLE: if (start_req) begin
            acc <= '0; done <= 1'b0; row_idx <= '0; remaining <= length_q;
            if (length_q == '0) begin state <= DRAIN; drain_cnt <= 2'd2; end
            else state <= STREAM;
          end
          STREAM: begin
            row_idx <= row_idx + 1'b1;
            if (remaining <= LEN_BITS'(P)) begin
              remaining <= '0; state <= DRAIN; drain_cnt <= 2'd2;
            end else remaining <= remaining - LEN_BITS'(P);
          end
          DRAIN: begin
            if (drain_cnt == 2'd0) begin
              result <= acc; done <= 1'b1; state <= IDLE;
            end else drain_cnt <= drain_cnt - 1'b1;
          end
          default: state <= IDLE;
        endcase
      end
    end
  end
endmodule
