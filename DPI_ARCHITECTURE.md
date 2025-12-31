# DPI-C Architecture: SystemVerilog ↔ C++ Interaction

## Overview

This document explains how the SystemVerilog transactor (`xactor.sv`) and C++ DPI-C implementation (`dpi_xactor.cpp`) work together to bridge hardware RTL with a C++ memory model.

## System Architecture

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐      ┌──────────────┐
│   RTL       │─────>│   XACTOR.V   │─────>│     C++     │─────>│   SPARSE     │
│  MASTER     │      │  (SystemVerilog)    │  (DPI-C)    │      │   MEMORY     │
│             │<─────│              │<─────│             │<─────│              │
└─────────────┘      └──────────────┘      └─────────────┘      └──────────────┘
```

The system implements a **read transaction flow** where:
1. **RTL Master** (`rtl_master.sv`) generates read requests
2. **XACTOR** (`xactor.sv`) translates SystemVerilog signals to C++ function calls
3. **C++ DPI** (`dpi_xactor.cpp`) manages transactions and simulates memory latency
4. **Sparse Memory** (inside C++) stores and retrieves data
5. Responses flow back through the same path

---

## Component Breakdown

### 1. RTL Master (`rtl_master.sv`)

**Purpose**: Hardware design that needs to read from memory

**Behavior**:
- Issues 4 sequential read requests (addresses: 0x1000, 0x1004, 0x1008, 0x100C)
- Uses ready/valid handshaking protocol
- State machine: `IDLE → SEND_REQ → WAIT_RESP → (repeat or IDLE)`

**Interface**:
- **Request side**: `req_valid`, `req_addr`, `req_id`, `req_ready`
- **Response side**: `resp_valid`, `resp_data`, `resp_id`, `resp_ready`

---

### 2. XACTOR (`xactor.sv`) - SystemVerilog Bridge

**Purpose**: Translates SystemVerilog signals to/from C++ DPI-C function calls

#### Key Components

##### DPI-C Imports (Lines 27-33)
```systemverilog
import "DPI-C" context task send_read (...);
import "DPI-C" context task recv_resp (...);
```
- Declares C++ functions that can be called from SystemVerilog
- `context` keyword means these functions can access SystemVerilog context (variables, etc.)

##### Request Path: RTL → C++ (Lines 50-62)

**Flow**:
1. Monitors `rtl_req_valid` and `rtl_req_ready` handshake
2. When handshake occurs, calls `send_read(cycle_count, rtl_req_addr, rtl_req_id)`
3. Always asserts `rtl_req_ready = 1` (can accept requests immediately)

**Code**:
```systemverilog
if (rtl_req_valid && rtl_req_ready) begin
    send_read(cycle_count, rtl_req_addr, rtl_req_id);
end
rtl_req_ready <= 1'b1;  // Always ready
```

##### Response Path: C++ → RTL (Lines 65-86)

**Flow**:
1. Every clock cycle, polls C++ for responses: `recv_resp(cycle_count, dpi_r_data, dpi_r_id, dpi_valid)`
2. If `dpi_valid` is true and no response is currently being held:
   - Captures the response data and ID
   - Asserts `rtl_resp_valid = 1`
3. When RTL accepts response (`rtl_resp_ready && rtl_resp_valid`):
   - Deasserts `rtl_resp_valid`

**Code**:
```systemverilog
recv_resp(cycle_count, dpi_r_data, dpi_r_id, dpi_valid);

if (dpi_valid && !rtl_resp_valid) begin
    rtl_resp_data <= dpi_r_data;
    rtl_resp_id <= dpi_r_id;
    rtl_resp_valid <= 1'b1;
end else if (rtl_resp_valid && rtl_resp_ready) begin
    rtl_resp_valid <= 1'b0;
end
```

**Key Point**: The transactor **polls** C++ every cycle, rather than using callbacks. This is a common pattern in DPI-C.

---

### 3. C++ DPI Implementation (`dpi_xactor.cpp`)

**Purpose**: Manages transactions, simulates variable memory latency, and implements sparse memory

#### Architecture Layers

##### Layer 1: Data Structures

**Transaction Struct** (Lines 25-34)
```cpp
struct Transaction {
    uint64_t s_addr;        // Memory address
    uint64_t s_id;          // Transaction ID
    uint64_t return_data;   // Data returned from memory
    uint64_t begin_cycle;   // Cycle when request started
    uint64_t latency;       // Simulated latency in cycles
    
    Transaction(...) { ... }  // Constructor with initializer list
};
```

**TransactionCompare** (Lines 38-44)
- Comparator for priority queue
- Sorts transactions by completion time (`begin_cycle + latency`)
- Enables **out-of-order response handling**

##### Layer 2: Singleton Class (Lines 48-127)

**Purpose**: Manages all transaction state and memory

**Data Members**:
- `HashTable`: Maps transaction ID → history of all transactions with that ID
- `pending_responses`: Priority queue of transactions waiting to complete
- `sparse_memory`: Hash map storing memory contents

**Key Methods**:

1. **`send_read()`** (Lines 70-84)
   - Called when SystemVerilog issues a read request
   - Calculates latency based on address: `latency = addr % 11`
   - Creates `Transaction` object
   - Pushes to `pending_responses` queue
   - Stores in `HashTable` for history tracking

2. **`recv_resp()`** (Lines 87-116) - **Member Function**
   - Checks if any transaction is ready to complete
   - Compares current cycle with `begin_cycle + latency`
   - If ready:
     - Reads data from sparse memory
     - Returns transaction ID and data
     - Removes from pending queue
   - Returns `bool`: `true` if response available, `false` otherwise

3. **`get_memory()`** (Lines 119-126)
   - Accesses sparse memory hash map
   - Returns stored value if address exists
   - Returns default value (`addr ^ 0xA5A5A5A5A5A5A5A5ULL`) if not found

##### Layer 3: DPI-C Wrapper Functions (Lines 140-156)

**Purpose**: C-compatible interface for SystemVerilog to call

**`send_read()`** (Lines 141-144)
```cpp
void send_read(uint64_t curr_cycle, uint64_t s_addr, uint64_t s_id) {
    init_transactions();  // Initialize singleton if needed
    Transactions->send_read(curr_cycle, s_addr, s_id);
}
```
- Wrapper around `Singleton::send_read()`
- Ensures singleton is initialized

**`recv_resp()`** (Lines 146-150)
```cpp
void recv_resp(uint64_t curr_cycle, uint64_t* r_data, uint64_t* r_id, svBit* valid) {
    init_transactions();
    bool resp_valid = Transactions->recv_resp(curr_cycle, r_data, r_id);
    *valid = resp_valid ? sv_1 : sv_0;  // Convert bool to SystemVerilog bit
}
```
- Wrapper around `Singleton::recv_resp()`
- Converts C++ `bool` return to SystemVerilog `svBit*` output
- This is why there are **two** `recv_resp` functions:
  - Member function: `bool recv_resp(...)` - does the work
  - DPI wrapper: `void recv_resp(..., svBit* valid)` - SystemVerilog interface

**`read_memory()`** (Lines 153-156)
- Helper function called by `recv_resp()` to access memory
- Also wrapped for singleton initialization

---

## Data Flow Example

### Request Flow: RTL → C++

1. **RTL Master** asserts `req_valid = 1`, `req_addr = 0x1000`, `req_id = 10`
2. **XACTOR** sees handshake (`req_valid && req_ready`)
3. **XACTOR** calls `send_read(cycle_count=1, 0x1000, 10)`
4. **C++ `send_read()`**:
   - Calculates latency: `0x1000 % 11 = 4 cycles`
   - Creates `Transaction(0x1000, 10, 1, 4)`
   - Pushes to `pending_responses` queue
   - Stores in `HashTable[10]`

### Response Flow: C++ → RTL

1. **XACTOR** calls `recv_resp(cycle_count=5, &dpi_r_data, &dpi_r_id, &dpi_valid)` every cycle
2. **C++ `recv_resp()`**:
   - Checks top of `pending_responses` queue
   - Calculates: `complete_cycle = 1 + 4 = 5`
   - Current cycle (5) >= complete_cycle (5) → **ready!**
   - Calls `read_memory(0x1000)` → returns `0xDEADBEEF`
   - Sets `*r_data = 0xDEADBEEF`, `*r_id = 10`, `*valid = true`
   - Pops transaction from queue
3. **XACTOR** receives `dpi_valid = 1`
4. **XACTOR** asserts `rtl_resp_valid = 1`, `rtl_resp_data = 0xDEADBEEF`, `rtl_resp_id = 10`
5. **RTL Master** sees `resp_valid = 1`, accepts response, deasserts `resp_valid`

---

## Key Design Patterns

### 1. Singleton Pattern
- Single global `Transactions` instance manages all state
- Initialized lazily via `init_transactions()`
- Ensures consistent state across all DPI calls

### 2. Priority Queue for Out-of-Order Responses
- Transactions complete in order of `begin_cycle + latency`, not request order
- Example: Request A (latency=8) and Request B (latency=2) → B completes first
- `TransactionCompare` ensures earliest completion is always at top of queue

### 3. Polling vs. Callbacks
- SystemVerilog **polls** C++ every cycle via `recv_resp()`
- C++ returns `valid = false` if no response ready
- Simpler than callback-based approach, works well for cycle-accurate simulation

### 4. DPI-C Wrapper Pattern
- C++ member functions cannot be called directly from SystemVerilog
- Wrapper functions in `extern "C"` block provide C-compatible interface
- Wrappers handle type conversions (e.g., `bool` → `svBit*`)

---

## Memory Model

### Sparse Memory
- Only stores specific addresses (0x1000, 0x1004, 0x1008, 0x100C)
- Other addresses return: `addr ^ 0xA5A5A5A5A5A5A5A5ULL`
- Implemented as `std::unordered_map<uint64_t, uint64_t>`

### Pre-initialized Values
```cpp
sparse_memory[0x1000] = 0xDEADBEEF;
sparse_memory[0x1004] = 0xCAFEBABE;
sparse_memory[0x1008] = 0x12345678;
sparse_memory[0x100C] = 0xABCDEF00;
```

---

## Latency Simulation

### Variable Latency
- Latency calculated as: `latency = address % 11`
- Simulates real-world memory with variable access times
- Different addresses have different latencies (0-10 cycles)

### Example from Simulation
- Address 0x1000: `0x1000 % 11 = 4 cycles`
- Address 0x1004: `0x1004 % 11 = 8 cycles`
- Address 0x1008: `0x1008 % 11 = 1 cycle`
- Address 0x100C: `0x100C % 11 = 5 cycles`

---

## Integration Points

### SystemVerilog → C++
- **Function calls**: `send_read()`, `recv_resp()`
- **Data types**: `longint unsigned` (64-bit) ↔ `uint64_t`
- **Context**: `context` keyword allows C++ to access SV variables

### C++ → SystemVerilog
- **Output parameters**: C++ modifies pointers passed from SV
- **Return values**: `svBit` for boolean status
- **Print statements**: `std::cout` appears in simulation log

---

## Summary

The system demonstrates a clean separation of concerns:
- **RTL** (`rtl_master.sv`): Hardware behavior
- **XACTOR** (`xactor.sv`): Protocol translation layer
- **C++** (`dpi_xactor.cpp`): Complex transaction management and memory modeling

The DPI-C interface allows SystemVerilog to leverage C++'s rich data structures (priority queues, hash maps) while maintaining cycle-accurate simulation semantics.

