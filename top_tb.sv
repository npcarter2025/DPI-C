/*
 * Top-level testbench that instantiates:
 * RTL -> XACTOR -> C++ -> SPARSE MEMORY
 */
module top_tb;

    logic clk;
    logic rst_n;
    
    // RTL to XACTOR interface
    logic        req_valid;
    logic [63:0] req_addr;
    logic [63:0] req_id;
    logic        req_ready;
    
    // XACTOR to RTL interface
    logic        resp_valid;
    logic [63:0] resp_data;
    logic [63:0] resp_id;
    logic        resp_ready;
    
    // Clock generation
    initial begin
        clk = 0;
        forever #5 clk = ~clk;
    end
    
    // Reset generation
    initial begin
        rst_n = 0;
        #20;
        rst_n = 1;
        $display("[%0t] Testbench: Reset released", $time);
    end
    
    // Instantiate RTL master
    rtl_master u_rtl_master (
        .clk(clk),
        .rst_n(rst_n),
        .req_valid(req_valid),
        .req_addr(req_addr),
        .req_id(req_id),
        .req_ready(req_ready),
        .resp_valid(resp_valid),
        .resp_data(resp_data),
        .resp_id(resp_id),
        .resp_ready(resp_ready)
    );
    
    // Instantiate transactor
    xactor u_xactor (
        .clk(clk),
        .rst_n(rst_n),
        .rtl_req_valid(req_valid),
        .rtl_req_addr(req_addr),
        .rtl_req_id(req_id),
        .rtl_req_ready(req_ready),
        .rtl_resp_valid(resp_valid),
        .rtl_resp_data(resp_data),
        .rtl_resp_id(resp_id),
        .rtl_resp_ready(resp_ready)
    );
    
    // Timeout
    initial begin
        #1000;
        $display("[%0t] Testbench: Timeout!", $time);
        $finish;
    end

endmodule

