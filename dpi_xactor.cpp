/*
///
________          ___________                ________             _________________
|       |         |          |               |       |            |               |
|  RTL  | ------->| XACTOR.V |-------------->| C++   | ---------->| SPARSE MEMORY |
|_______|         |__________|               |_______|            | ______________|
*/

#include "svdpi.h"
#include <queue>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <cstdint>

extern "C" {

// Forward declaration for sparse memory
uint64_t read_memory(uint64_t addr);

// Transaction ID type
typedef uint64_t T_id;

// Transaction structure to track a single transaction's details
struct Transaction {
    uint64_t s_addr;
    uint64_t s_id;
    uint64_t return_data;
    uint64_t begin_cycle;
    uint64_t latency;  // delay in cycles
    
    Transaction(uint64_t addr, uint64_t id, uint64_t cycle, uint64_t delay)
        : s_addr(addr), s_id(id), begin_cycle(cycle), latency(delay), return_data(0) {}
};

// Comparator for priority queue: sort by completion time (begin_cycle + latency)
// This handles out-of-order responses - responses come back sorted by completion time
struct TransactionCompare {
    bool operator()(const Transaction& a, const Transaction& b) {
        uint64_t a_complete = a.begin_cycle + a.latency;
        uint64_t b_complete = b.begin_cycle + b.latency;
        return a_complete > b_complete;  // min-heap: earliest completion first
    }
};

// Singleton struct to manage transactions (as per original design)
// Transactions = singleton instance that manages collections of Transaction (individual transactions)
struct Singleton {
    std::unordered_map<T_id, std::vector<Transaction>> HashTable;  // Map ID to transaction history (vector of Transaction)
    std::priority_queue<Transaction, std::vector<Transaction>, TransactionCompare> pending_responses;  // Queue of Transaction
    std::unordered_map<uint64_t, uint64_t> sparse_memory;  // sparse memory model
    
    // Generate latency based on address (simulates variable memory access time)
    uint64_t get_latency(uint64_t addr) {
        // Simple hash-based latency: 0-10 cycles
        // Example: (id, delay) = (10, 5), (4,2), (9,0), (3,3)
        // Output sorted by delay: (9,0), (4,2), (3,3), (10,5)
        return (addr % 11);
    }
    
    // Initialize sparse memory
    void init_memory() {
        sparse_memory[0x1000] = 0xDEADBEEF;
        sparse_memory[0x1004] = 0xCAFEBABE;
        sparse_memory[0x1008] = 0x12345678;
        sparse_memory[0x100C] = 0xABCDEF00;
    }
    
    // Send read transaction
    void send_read(uint64_t curr_cycle, uint64_t s_addr, uint64_t s_id) {
        uint64_t latency = get_latency(s_addr);
        
        std::cout << "[C++] send_read: cycle=" << curr_cycle 
                  << " addr=0x" << std::hex << s_addr << std::dec
                  << " id=" << s_id 
                  << " latency=" << latency << " cycles" << std::endl;
        
        // Create transaction and add to pending queue
        Transaction txn(s_addr, s_id, curr_cycle, latency);
        pending_responses.push(txn);
        
        // Also store in hash table for transaction history tracking
        HashTable[s_id].push_back(txn);
    }
    
    // Receive response (check if any transaction is ready)
    bool recv_resp(uint64_t curr_cycle, uint64_t* r_data, uint64_t* r_id) {
        if (pending_responses.empty()) {
            return false;
        }
        
        // Check if top transaction is ready
        Transaction top = pending_responses.top();
        uint64_t complete_cycle = top.begin_cycle + top.latency;
        
        if (curr_cycle >= complete_cycle) {
            // Transaction is ready
            *r_id = top.s_id;
            *r_data = read_memory(top.s_addr);
            
            // Update the transaction in hash table with return data
            if (HashTable.find(top.s_id) != HashTable.end() && !HashTable[top.s_id].empty()) {
                HashTable[top.s_id].back().return_data = *r_data;
            }
            
            std::cout << "[C++] recv_resp: cycle=" << curr_cycle
                      << " id=" << *r_id
                      << " data=0x" << std::hex << *r_data << std::dec
                      << " (latency=" << (curr_cycle - top.begin_cycle) << " cycles)" << std::endl;
            
            pending_responses.pop();
            return true;
        }
        
        return false;
    }
    
    // Accessor for sparse memory (used by read_memory)
    uint64_t get_memory(uint64_t addr) {
        auto it = sparse_memory.find(addr);
        if (it != sparse_memory.end()) {
            return it->second;
        }
        // Return default value if address not found
        return addr ^ 0xA5A5A5A5A5A5A5A5ULL;
    }
};

// Global singleton instance (as per original design)
static Singleton* Transactions = nullptr;

// Initialize singleton
void init_transactions() {
    if (Transactions == nullptr) {
        Transactions = new Singleton();
        Transactions->init_memory();
    }
}

// DPI-C functions
void send_read(uint64_t curr_cycle, uint64_t s_addr, uint64_t s_id) {
    init_transactions();
    Transactions->send_read(curr_cycle, s_addr, s_id);
}

void recv_resp(uint64_t curr_cycle, uint64_t* r_data, uint64_t* r_id, svBit* valid) {
    init_transactions();
    bool resp_valid = Transactions->recv_resp(curr_cycle, r_data, r_id);
    *valid = resp_valid ? sv_1 : sv_0;
}

// Sparse memory read function
uint64_t read_memory(uint64_t addr) {
    init_transactions();
    return Transactions->get_memory(addr);
}

} // extern "C"
