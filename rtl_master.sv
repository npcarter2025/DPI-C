/*
 * RTL Module - Simple memory master that issues read requests
 * This represents the hardware design that needs to read from memory
 */
module rtl_master (
    input  logic clk,
    input  logic rst_n,
    
    // Interface to transactor (simple ready/valid protocol)
    output logic        req_valid,
    output logic [63:0] req_addr,
    output logic [63:0] req_id,
    input  logic        req_ready,
    
    input  logic        resp_valid,
    input  logic [63:0] resp_data,
    input  logic [63:0] resp_id,
    output logic        resp_ready
);

    // State machine for issuing reads
    typedef enum logic [1:0] {
        IDLE,
        SEND_REQ,
        WAIT_RESP
    } state_t;
    
    state_t state, next_state;
    logic [63:0] addr_counter;
    logic [63:0] txn_id_counter;
    int read_count;
    
    // State register
    always_ff @(posedge clk) begin
        if (!rst_n) begin
            state <= IDLE;
            addr_counter <= 64'h1000;
            txn_id_counter <= 64'd10;
            read_count <= 0;
        end else begin
            state <= next_state;
            if (req_valid && req_ready) begin
                addr_counter <= addr_counter + 64'h4;
                txn_id_counter <= txn_id_counter + 1;
                read_count <= read_count + 1;
            end
        end
    end
    
    // Next state logic
    always_comb begin
        next_state = state;
        req_valid = 1'b0;
        req_addr = addr_counter;
        req_id = txn_id_counter;
        resp_ready = 1'b1;
        
        case (state)
            IDLE: begin
                if (read_count < 4) begin
                    next_state = SEND_REQ;
                end
            end
            
            SEND_REQ: begin
                req_valid = 1'b1;
                if (req_ready) begin
                    next_state = WAIT_RESP;
                end
            end
            
            WAIT_RESP: begin
                if (resp_valid) begin
                    $display("[%0t] RTL: Received response id=%0d data=0x%0h", 
                             $time, resp_id, resp_data);
                    if (read_count < 4) begin
                        next_state = SEND_REQ;
                    end else begin
                        next_state = IDLE;
                    end
                end
            end
        endcase
    end

endmodule

