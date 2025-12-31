/*
///
________          ___________                ________             _________________
|       |         |          |               |       |            |               |
|  RTL  | ------->| XACTOR.V |-------------->| C++   | ---------->| SPARSE MEMORY |
|_______|         |__________|               |_______|            | ______________|
*/

// XACTOR.V - Transactor that bridges RTL interface to C++ DPI-C calls
module xactor (
    input  logic clk,
    input  logic rst_n,
    
    // RTL Interface (ready/valid protocol)
    input  logic        rtl_req_valid,
    input  logic [63:0] rtl_req_addr,
    input  logic [63:0] rtl_req_id,
    output logic        rtl_req_ready,
    
    output logic        rtl_resp_valid,
    output logic [63:0] rtl_resp_data,
    output logic [63:0] rtl_resp_id,
    input  logic        rtl_resp_ready
);

  // DPI imports: implemented in C
  import "DPI-C" context task send_read (input longint unsigned curr_cycle, 
                                         input longint unsigned s_addr, 
                                         input longint unsigned s_id);
  import "DPI-C" context task recv_resp (input longint unsigned curr_cycle,
                                         output longint unsigned r_data,
                                         output longint unsigned r_id,
                                         output bit valid);

  longint unsigned cycle_count;
  longint unsigned dpi_r_data;
  longint unsigned dpi_r_id;
  bit dpi_valid;
  
  // Cycle counter
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      cycle_count <= 0;
    end else begin
      cycle_count <= cycle_count + 1;
    end
  end
  
  // Request side: RTL -> C++
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      rtl_req_ready <= 1'b0;
    end else begin
      // Accept request when valid and ready
      if (rtl_req_valid && rtl_req_ready) begin
        $display("[%0t] XACTOR: Forwarding read request to C++: addr=0x%0h id=%0d", 
                 $time, rtl_req_addr, rtl_req_id);
        send_read(cycle_count, rtl_req_addr, rtl_req_id);
      end
      rtl_req_ready <= 1'b1;  // Always ready (can accept immediately)
    end
  end
  
  // Response side: C++ -> RTL
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      rtl_resp_valid <= 1'b0;
      rtl_resp_data <= 64'h0;
      rtl_resp_id <= 64'h0;
    end else begin
      // Poll C++ for responses
      recv_resp(cycle_count, dpi_r_data, dpi_r_id, dpi_valid);
      
      if (dpi_valid && !rtl_resp_valid) begin
        // New response available
        rtl_resp_data <= dpi_r_data;
        rtl_resp_id <= dpi_r_id;
        rtl_resp_valid <= 1'b1;
        $display("[%0t] XACTOR: Forwarding response from C++: id=%0d data=0x%0h", 
                 $time, dpi_r_id, dpi_r_data);
      end else if (rtl_resp_valid && rtl_resp_ready) begin
        // Response accepted by RTL
        rtl_resp_valid <= 1'b0;
      end
    end
  end

endmodule
