# Building a Cycle-Accurate Simulator

This document is a **practical, industry-aligned guide** to building a **cycle-accurate simulation tool**, similar in spirit to what Platform Architecture teams (e.g., Apple) use for architectural exploration and performance modeling.

---

## 1. What “Cycle-Accurate” Really Means

A cycle-accurate simulator guarantees:

- Time advances **one clock cycle at a time**
- All state updates occur **only on clock edges**
- Latencies are modeled in **integer cycles**
- Behavior is **deterministic**

Non-goals (by design):
- No delta cycles
- No event queues
- No glitch or combinational timing modeling
- No bit-level RTL quirks

> The simulator answers *“what happens each cycle?”*, not *“what happens in every delta?”*

---

## 2. Where Cycle-Accurate Simulation Fits

```
High-level functional models
        ↓
Cycle-accurate simulation   ← Platform Architecture focus
        ↓
RTL simulation (VCS / Xcelium)
        ↓
Emulation (Palladium / ZeBu)
        ↓
Silicon
```

Cycle-accurate simulators are used to:
- Explore microarchitecture tradeoffs
- Estimate performance and throughput
- Tune cache sizes, queue depths, pipelines
- Run long workloads before RTL exists

---

## 3. Core Design Principle (Non-Negotiable)

### Two-phase update per cycle

Every simulated cycle has exactly two phases:

1. **Evaluate next state** (combinational logic)
2. **Commit state** (clock edge)

No component can see another component’s next state in the same cycle.

This mirrors real hardware behavior and avoids races.

---

## 4. Core Architecture

### Component Interface

Each modeled block implements a simple interface:

```cpp
struct Component {
    virtual void eval() = 0;   // compute next state
    virtual void commit() = 0; // latch on clock edge
};
```

---

### Global Simulation Loop

```cpp
std::vector<Component*> components;

for (uint64_t cycle = 0; cycle < max_cycles; cycle++) {
    for (auto* c : components)
        c->eval();

    for (auto* c : components)
        c->commit();
}
```

This loop **is the simulator**.
Everything else is tooling, optimization, or modeling detail.

---

## 5. Example: Pipeline Register

```cpp
struct PipeReg : Component {
    int cur;
    int next;

    void eval() override {
        next = cur + 1;
    }

    void commit() override {
        cur = next;
    }
};
```

- `eval()` = combinational logic
- `commit()` = clock edge

---

## 6. Modeling Latency (Critical)

Cycle-accurate simulators live and die by **explicit latency modeling**.

### Fixed-Latency Queue Example

```cpp
struct DelayQueue {
    std::deque<int> q;
    int latency;

    void push(int v) {
        q.push_back(v);
    }

    bool pop(int& out) {
        if (q.size() > latency) {
            out = q.front();
            q.pop_front();
            return true;
        }
        return false;
    }
};
```

This allows you to say:
> “This operation takes exactly N cycles.”

---

## 7. Determinism & Ordering Rules

To remain cycle-accurate and reproducible:

- Evaluation order is fixed
- All `eval()` calls happen before any `commit()`
- No component can read another’s next-state
- No time advances within a cycle

This is conceptually equivalent to:
- Nonblocking assignments in RTL
- Clocked hardware semantics

---

## 8. Why This Is Faster Than RTL Simulation

RTL simulators:
- Event-driven
- Delta cycles
- Sensitivity lists
- Fine-grained signal activity

Cycle-accurate simulators:
- No event queue
- No deltas
- Coarse-grained state updates
- Plain C/C++ data structures

Result: **orders-of-magnitude faster** for long-running workloads.

---

## 9. Validation Strategy (How Teams Do This)

Cycle-accurate simulators are validated by:

1. Running small, directed tests
2. Comparing against RTL **cycle-by-cycle**
3. Accepting early functional mismatch
4. Converging on performance accuracy

Golden rule:
> Architectural simulators predict **trends and performance**, not RTL quirks.

---

## 10. What These Tools Usually Grow Into

Over time, teams add:

- Performance counters
- Trace dumping
- Assertions
- Checkpoint / restore
- Statistics and profiling

But the **core two-phase loop never changes**.

---

## 11. How This Maps to Platform Architecture Roles

Platform Architecture teams use cycle-accurate simulators to:

- Evaluate design choices before RTL exists
- Answer "how many cycles does this take?"
- Predict system-level performance
- Guide RTL and physical design decisions

This is **not traditional DV** — it is architectural modeling.

---

## 12. Recommended Starter Project

Build a minimal system:

- Simple CPU issuing requests
- Cache with hit/miss latency
- Memory controller

Then:
- Count cycles
- Vary latency parameters
- Measure throughput

This exercise alone demonstrates **real platform-architecture thinking**.

---

## 13. Key Takeaway

> A cycle-accurate simulator is a deterministic, clock-stepped state machine model designed to explore performance and architectural tradeoffs long before RTL and emulation.

---

*End of document*

