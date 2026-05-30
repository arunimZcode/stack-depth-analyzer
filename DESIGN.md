# Design Document — Stack Depth Analyzer

## Problem Statement

Embedded and RTOS developers need to know the **total stack consumed by a call chain**, not just per-function frame sizes. Stack overflow in embedded systems causes silent memory corruption with no error message — the CPU writes past the stack boundary into adjacent memory (heap, globals, other task stacks), causing crashes that appear unrelated to the actual cause.

Existing tools fall short:

| Tool | What it provides | Missing |
|---|---|---|
| GCC `-fstack-usage` | Per-function frame size | No call chain analysis |
| Clang `-Wframe-larger-than` | Per-frame warning | No cumulative depth |
| Manual inspection | Ad-hoc | Error-prone, doesn't scale |
| This tool | Worst-case cumulative depth per entry point | — |

---

## Design Goals

1. **Whole-program analysis** — must see all functions and their relationships
2. **Conservative (safe) estimates** — never underestimate stack usage
3. **Actionable output** — tell developers exactly which path causes the problem
4. **Configurable threshold** — different targets have different stack budgets
5. **Zero runtime overhead** — purely static, runs at compile time

---

## Architecture Overview

```
Source Code (.c)
      │
      │  clang -emit-llvm (Clang frontend)
      ▼
  LLVM IR (.bc)
      │
      ├──► Frame Estimator (ModulePass)
      │    - walks alloca instructions per function
      │    - builds FrameSizeMap: fn → bytes
      │
      ├──► CallGraphWrapperPass (LLVM built-in)
      │    - constructs static call graph from IR
      │
      └──► CallChainAnalyzer (ModulePass)
           - DFS traversal of call graph
           - accumulates frame sizes per path
           - reports worst-case chains
           - flags threshold violations
```

---

## Key Design Decisions

### Decision 1: IR-level analysis vs MachineFunction-level

**Options considered:**
- **IR level (alloca instructions)** ← chosen
- MachineFunction level (post-register-allocation)

**Rationale:** IR-level analysis is portable, doesn't require a target backend, and runs earlier in the pipeline. `alloca` instructions directly represent programmer-visible stack allocations. MachineFunction would capture spills too, but requires a full backend compile for each target architecture — impractical for a general tool.

**Tradeoff:** We miss register spills and compiler-generated temporaries. Estimates are slightly conservative (may undercount) but never incorrect in the unsafe direction for programmer-allocated buffers.

---

### Decision 2: ModulePass over FunctionPass

**Options considered:**
- FunctionPass — runs per function independently
- **ModulePass** ← chosen — sees entire program at once

**Rationale:** Call graph analysis is inherently whole-program. A FunctionPass cannot see who calls whom. The ModulePass processes all functions in one invocation, allowing us to build the FrameSizeMap first, then traverse call chains using complete information.

---

### Decision 3: DFS with cycle detection for recursion

**Options considered:**
- Simple BFS traversal
- **DFS with visited set** ← chosen
- Strongly connected components (Tarjan's algorithm)

**Rationale:** DFS naturally tracks the current path being explored, making it easy to accumulate stack depth along a path and record the worst-case path. The visited set detects recursion (cycles) in O(1) per node. Tarjan's would be more complete for SCC analysis but adds complexity without benefit for our use case (we just need to detect and warn, not analyze recursive depth).

---

### Decision 4: Static call graph (no dynamic dispatch handling)

**Options considered:**
- Static call graph only ← chosen (with conservative bound for indirect calls)
- Points-to analysis for function pointers
- Whole-program devirtualization

**Rationale:** Full points-to analysis (e.g. Andersen's or Steensgaard's) adds significant complexity. For embedded/RTOS code, dynamic dispatch is rare — most calls are direct. Indirect calls (function pointers) are flagged with a configurable upper bound (default 512B). This is conservative and correct.

---

### Decision 5: Configurable threshold

The threshold is a CLI parameter (`--threshold=N`) rather than hardcoded. This is critical because stack budgets vary:

- Microcontroller ISR: 256–512 bytes
- FreeRTOS task: 1–8 KB  
- Linux kernel thread: 8 KB
- Userspace thread: 1–8 MB

---

## Alternatives Considered and Rejected

### Alternative: LLVM StackSafetyAnalysis pass
LLVM has an internal `StackSafetyAnalysis` but it focuses on whether stack objects escape (for security), not cumulative depth across call chains.

### Alternative: Source-level analysis (Clang AST)
Could analyze C source directly via Clang's AST. Rejected because:
- Misses inlining decisions made by the optimizer
- Doesn't account for compiler-generated stack usage
- IR is the right level for this analysis

### Alternative: Binary analysis (post-link)
Tools like `pstack` or binary disassembly could measure actual frame sizes. Rejected because:
- Requires a complete build for the target architecture
- Can't run during development before hardware is available
- Our tool runs during compilation, catching issues earlier

---

## Limitations

1. **Dynamic allocation** — `malloc()`/heap usage is not tracked (by design — heap != stack)
2. **Indirect calls** — function pointers use a conservative fixed bound
3. **Recursion** — detected and warned but not depth-bounded (unbounded recursion = unbounded stack)
4. **Inlining** — inlined functions disappear from the IR; their stack is attributed to the caller (actually correct behavior)
5. **Variable-length arrays (VLAs)** — dynamic alloca cannot be statically bounded; reported as unknown
