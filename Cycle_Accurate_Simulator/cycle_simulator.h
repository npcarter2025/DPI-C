/*
 * Cycle-Accurate Simulator Core
 * 
 * Core architecture: Two-phase evaluation (eval/commit) per cycle
 */

#ifndef CYCLE_SIMULATOR_H
#define CYCLE_SIMULATOR_H

#include <vector>
#include <cstdint>
#include <iostream>

// Base class for all simulated components
struct Component {
    virtual ~Component() = default;
    virtual void eval() = 0;   // Compute next state (combinational logic)
    virtual void commit() = 0; // Update state (clock edge)
};

// The simulator engine - this is the core of cycle-accurate simulation
class CycleSimulator {
private:
    std::vector<Component*> components;
    uint64_t current_cycle;
    uint64_t max_cycles;

public:
    CycleSimulator(uint64_t max_cycles = 1000) 
        : current_cycle(0), max_cycles(max_cycles) {}
    
    ~CycleSimulator() {
        // Clean up components
        for (auto* c : components) {
            delete c;
        }
    }
    
    // Register a component to be simulated
    void add_component(Component* comp) {
        components.push_back(comp);
    }
    
    // Run the simulation
    void run() {
        std::cout << "Starting cycle-accurate simulation..." << std::endl;
        
        for (current_cycle = 0; current_cycle < max_cycles; current_cycle++) {
            // Phase 1: Evaluate all components (combinational logic)
            for (auto* c : components) {
                c->eval();
            }
            
            // Phase 2: Commit all state updates (clock edge)
            for (auto* c : components) {
                c->commit();
            }
        }
        
        std::cout << "Simulation complete after " << current_cycle << " cycles" << std::endl;
    }
    
    uint64_t get_cycle() const { return current_cycle; }
    void set_max_cycles(uint64_t max) { max_cycles = max; }
};

#endif // CYCLE_SIMULATOR_H

