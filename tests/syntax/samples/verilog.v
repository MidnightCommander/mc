// Verilog syntax highlighting sample file
// Exercises all TS captures: keyword, comment, string,
// delimiter, delimiter.special

`timescale 1ns / 1ps

// Module with parameters
module counter #(
    parameter WIDTH = 8,
    parameter MAX_COUNT = 255
) (
    input wire clk,
    input wire rst_n,
    input wire enable,
    output reg [WIDTH-1:0] count,
    output wire overflow,
    inout wire [7:0] data_bus
);

// Internal declarations
wire [WIDTH-1:0] next_count;
reg [WIDTH-1:0] prev_count;
integer i;
real delay_val;
localparam HALF = MAX_COUNT / 2;

// Continuous assignment with operators
assign next_count = count + 1;
assign overflow = (count == MAX_COUNT) ? 1'b1 : 1'b0;

// Always block with posedge/negedge
always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
        count <= {WIDTH{1'b0}};
        prev_count <= {WIDTH{1'b0}};
    end else if (enable) begin
        prev_count <= count;
        if (count == MAX_COUNT)
            count <= {WIDTH{1'b0}};
        else
            count <= next_count;
    end
end

// Combinational always block with case
always @(*) begin
    case (count[1:0])
        2'b00: $display("Phase 0");
        2'b01: $display("Phase 1");
        default: $display("Other");
    endcase
end

endmodule

// ALU module with function, operators
module alu (
    input wire [7:0] a, b,
    input wire [2:0] op,
    output reg [7:0] result
);

function [8:0] add_fn;
    input [7:0] x, y;
    begin add_fn = x + y; end
endfunction

always @(*) begin
    case (op)
        3'd0: result = a + b;
        3'd1: result = a - b;
        3'd2: result = a * b;
        3'd3: result = a / b;
        3'd4: result = a % b;
        3'd5: result = (a == b) ? 8'd1 : 8'd0;
        3'd6: result = (a != b) ? 8'd1 : 8'd0;
        default: result = 8'd0;
    endcase
end

// Bitwise and logical operators
wire [7:0] bw = (a & b) | (a ^ b) | ~a;
wire lg = (a > 0) && (b > 0) || !(a == b);
wire [7:0] sh = (a << 2) >> 2;

endmodule

// Task definition
module test_bench;

reg clk;
reg rst_n;
wire [7:0] cnt;

counter #(.WIDTH(8), .MAX_COUNT(255)) uut (
    .clk(clk), .rst_n(rst_n), .enable(1'b1),
    .count(cnt), .overflow(), .data_bus()
);

task toggle_reset;
    begin rst_n = 1'b0; #10 rst_n = 1'b1; end
endtask

initial begin
    clk = 1'b0;
    rst_n = 1'b0;
    toggle_reset;
    $display("Simulation started at %0t", $time);
    #1000 $finish;
end

// Clock generation with forever
always begin
    #5 clk = ~clk;
end

// Generate block
genvar gi;
generate
    for (gi = 0; gi < 4; gi = gi + 1) begin : gen_blk
        wire [7:0] tap = cnt >> gi;
    end
endgenerate

// String usage and specify block
initial begin
    $display("Test: %s", "hello world");
    $monitor("cnt=%d time=%0t", cnt, $time);
end
/* Block comment spanning multiple lines */
specify (clk => count) = (2, 3); endspecify

// Net types and gate primitives
wire signed [7:0] s_wire;
tri tri_net; wand wand_net; wor wor_net;
supply0 gnd; supply1 vdd;
and g1 (out1, in1, in2); or g2 (out2, in1, in2);
not g3 (out3, in1); nand g4 (out4, in1, in2);
nor g5 (out5, in1, in2); xor g6 (out6, in1, in2);
xnor g7 (out7, in1, in2); buf g8 (out8, in1);

// Force, release, disable, wait, event, defparam
event done_event;
initial begin
    force s_wire = 8'h42;
    #10 release s_wire;
    wait (cnt > 100); disable gen_blk;
    -> done_event;
end
defparam uut.WIDTH = 16;

endmodule
