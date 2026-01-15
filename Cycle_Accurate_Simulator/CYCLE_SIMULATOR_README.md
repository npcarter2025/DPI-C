# Cycle-Accurate Simulator Example

This directory contains a simple, working example of a cycle-accurate simulator based on the principles in `building_a_cycle_accurate_simulator_practical_guide.md`.

## What This Demonstrates

This example shows the **core architecture** of cycle-accurate simulation:

1. **Two-Phase Evaluation**: Every cycle has two phases:
   - `eval()`: Compute next state (combinational logic)
   - `commit()`: Update state (clock edge)

2. **Component-Based Architecture**: All simulated blocks inherit from `Component` base class

3. **Latency Modeling**: Using delay queues to model fixed-latency operations

## Files

- **`cycle_simulator.h`**: Core simulator engine and Component base class
- **`simple_example.cpp`**: Three working examples demonstrating the concepts
- **`Makefile.cycle_sim`**: Build file for compiling the example

## Building and Running

```bash
# Compile
make -f Makefile.cycle_sim

# Run
make -f Makefile.cycle_sim run

# Or directly:
./cycle_sim_example
```

## Examples Included

### Example 1: Simple Counter
Demonstrates the basic eval/commit pattern with a simple counter that increments each cycle.

### Example 2: Pipeline Register
Shows how pipeline stages work - computes next value in `eval()`, latches in `commit()`.

### Example 3: Request Generator → Delay Queue → Memory Controller
A complete system showing:
- Request generator (CPU) creating memory requests
- Delay queue modeling 5-cycle memory latency
- Memory controller processing requests after latency

## Key Concepts

### The Core Simulation Loop

```cpp
for (uint64_t cycle = 0; cycle < max_cycles; cycle++) {
    // Phase 1: Evaluate all components
    for (auto* c : components)
        c->eval();
    
    // Phase 2: Commit all state updates
    for (auto* c : components)
        c->commit();
}
```

This loop **is the simulator**. Everything else is modeling detail.

### Creating a Component

```cpp
struct MyComponent : Component {
    int cur_state;
    int next_state;
    
    void eval() override {
        // Compute next_state based on cur_state and inputs
        next_state = cur_state + 1;
    }
    
    void commit() override {
        // Update cur_state on clock edge
        cur_state = next_state;
    }
};
```

### Critical Rules

1. **No component can read another's `next_state` during `eval()`**
2. **All `eval()` calls happen before any `commit()`**
3. **Evaluation order is fixed and deterministic**
4. **Time only advances between cycles, not within a cycle**

## Next Steps

To build a more complete simulator:

1. **Add more components**: CPU, cache, memory, buses
2. **Model variable latency**: Use priority queues (like the DPI-C example)
3. **Add performance counters**: Track cycles, throughput, latency
4. **Add tracing**: Log transactions for analysis
5. **Add assertions**: Validate correct behavior

## Comparison with DPI-C Example

The DPI-C example in this directory (`dpi_xactor.cpp`) uses SystemVerilog's event-driven simulator with C++ helpers. This cycle-accurate simulator example is a **pure C++ implementation** that runs independently without SystemVerilog.

**DPI-C approach**: SystemVerilog drives simulation, C++ provides models
**This approach**: C++ drives simulation, all components are C++

Both are valid - choose based on whether you need RTL integration (DPI-C) or standalone performance modeling (pure C++).

