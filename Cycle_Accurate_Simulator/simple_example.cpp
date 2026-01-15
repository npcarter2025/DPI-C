/*
 * Simple Cycle-Accurate Simulator Example
 * 
 * Demonstrates:
 * - Basic eval/commit pattern
 * - Pipeline register
 * - Counter component
 * - Delay queue (latency modeling)
 */

#include "cycle_simulator.h"
#include <iostream>
#include <deque>
#include <queue>

// Example 1: Simple Counter
struct Counter : Component {
    uint64_t count;
    uint64_t next_count;
    
    Counter() : count(0), next_count(0) {}
    
    void eval() override {
        // Combinational logic: compute next value
        next_count = count + 1;
    }
    
    void commit() override {
        // Clock edge: update state
        count = next_count;
    }
    
    uint64_t get_count() const { return count; }
};

// Example 2: Pipeline Register (from the guide)
struct PipeReg : Component {
    int cur;
    int next;
    
    PipeReg(int initial = 0) : cur(initial), next(initial) {}
    
    void eval() override {
        next = cur + 1;  // Simple increment function
    }
    
    void commit() override {
        cur = next;  // Latch on clock edge
    }
    
    int get_value() const { return cur; }
};

// Example 3: Delay Queue (fixed latency modeling)
template<typename T>
struct DelayQueue : Component {
    std::deque<T> queue;
    uint64_t latency;
    
    DelayQueue(uint64_t latency_cycles) : latency(latency_cycles) {}
    
    void push(T value) {
        queue.push_back(value);
    }
    
    bool pop(T& out) {
        // Can only pop if we've waited at least 'latency' cycles
        if (queue.size() > latency) {
            out = queue.front();
            queue.pop_front();
            return true;
        }
        return false;
    }
    
    // Component interface (delay queue doesn't need eval/commit for basic operation,
    // but we implement it for consistency)
    void eval() override {
        // No combinational logic needed
    }
    
    void commit() override {
        // No state update needed (queue operations happen externally)
    }
    
    size_t size() const { return queue.size(); }
};

// Example 4: Request Generator (simulates a CPU generating memory requests)
struct RequestGenerator : Component {
    uint64_t req_count;
    uint64_t next_req_count;
    DelayQueue<uint64_t>* output_queue;
    uint64_t max_requests;
    
    RequestGenerator(DelayQueue<uint64_t>* queue, uint64_t max_req = 10)
        : req_count(0), next_req_count(0), output_queue(queue), max_requests(max_req) {}
    
    void eval() override {
        // Generate a new request if we haven't reached the limit
        if (req_count < max_requests) {
            next_req_count = req_count + 1;
        } else {
            next_req_count = req_count;  // Stop generating
        }
    }
    
    void commit() override {
        if (req_count < max_requests) {
            // Send request to delay queue
            output_queue->push(req_count * 0x1000);  // Generate addresses
            std::cout << "[Cycle " << req_count << "] RequestGenerator: Sent request #" 
                      << req_count << " (addr=0x" << std::hex << (req_count * 0x1000) 
                      << std::dec << ")" << std::endl;
        }
        req_count = next_req_count;
    }
    
    bool is_done() const { return req_count >= max_requests; }
};

// Example 5: Memory Controller (processes requests after latency)
struct MemoryController : Component {
    DelayQueue<uint64_t>* input_queue;
    uint64_t processed_count;
    uint64_t next_processed_count;
    
    MemoryController(DelayQueue<uint64_t>* queue)
        : input_queue(queue), processed_count(0), next_processed_count(0) {}
    
    void eval() override {
        uint64_t addr;
        if (input_queue->pop(addr)) {
            next_processed_count = processed_count + 1;
        } else {
            next_processed_count = processed_count;
        }
    }
    
    void commit() override {
        if (next_processed_count > processed_count) {
            std::cout << "[Cycle " << processed_count << "] MemoryController: Processed request" << std::endl;
        }
        processed_count = next_processed_count;
    }
    
    uint64_t get_processed() const { return processed_count; }
};

// Helper function to run a manual simulation loop
void run_manual_loop(std::vector<Component*>& components, uint64_t max_cycles) {
    for (uint64_t cycle = 0; cycle < max_cycles; cycle++) {
        // Eval phase
        for (auto* c : components) {
            c->eval();
        }
        
        // Commit phase
        for (auto* c : components) {
            c->commit();
        }
    }
}

// Main example program
int main() {
    std::cout << "=== Cycle-Accurate Simulator Example ===" << std::endl;
    std::cout << std::endl;
    
    // Example 1: Simple counter
    std::cout << "--- Example 1: Simple Counter ---" << std::endl;
    {
        CycleSimulator sim1(50);  // Run for 50 cycles
        Counter* counter = new Counter();
        sim1.add_component(counter);
        sim1.run();
        std::cout << "Final counter value: " << counter->get_count() << std::endl;
        // sim1 destructor will delete counter
    }
    std::cout << std::endl;
    
    // Example 2: Pipeline register
    std::cout << "--- Example 2: Pipeline Register ---" << std::endl;
    {
        PipeReg pipe(10);
        std::vector<Component*> components2;
        components2.push_back(&pipe);
        
        for (uint64_t i = 0; i < 5; i++) {
            // Eval phase
            for (auto* c : components2) c->eval();
            std::cout << "After eval (cycle " << i << "): next=" << pipe.next << std::endl;
            
            // Commit phase
            for (auto* c : components2) c->commit();
            std::cout << "After commit: cur=" << pipe.get_value() << std::endl;
        }
    }
    std::cout << std::endl;
    
    // Example 3: Request generator + Delay queue + Memory controller
    std::cout << "--- Example 3: Request Generator -> Delay Queue -> Memory Controller ---" << std::endl;
    {
        // Create delay queue with 5-cycle latency
        DelayQueue<uint64_t>* delay_queue = new DelayQueue<uint64_t>(5);
        RequestGenerator* gen = new RequestGenerator(delay_queue, 5);
        MemoryController* mem_ctrl = new MemoryController(delay_queue);
        
        std::vector<Component*> components3;
        components3.push_back(gen);
        components3.push_back(delay_queue);
        components3.push_back(mem_ctrl);
        
        // Manual simulation loop for better visibility
        uint64_t cycle = 0;
        while (cycle < 30 && (!gen->is_done() || delay_queue->size() > 5)) {
            // Eval phase
            for (auto* c : components3) c->eval();
            
            // Commit phase
            for (auto* c : components3) c->commit();
            
            cycle++;
        }
        
        std::cout << "Total requests processed: " << mem_ctrl->get_processed() << std::endl;
        std::cout << "Remaining in queue: " << delay_queue->size() << std::endl;
        
        // Manual cleanup for example 3 since we're not using CycleSimulator
        delete gen;
        delete delay_queue;
        delete mem_ctrl;
    }
    std::cout << std::endl;
    
    std::cout << "=== Examples Complete ===" << std::endl;
    
    return 0;
}
