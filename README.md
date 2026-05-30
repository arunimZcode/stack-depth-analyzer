# Stack Depth Analyzer

An LLVM analysis pass that statically estimates **worst-case cumulative stack usage** across entire call chains — built for embedded and RTOS development where stack overflow is the #1 cause of mysterious crashes.

## The Problem

GCC's `-fstack-usage` tells you `b_func uses 256 bytes` but not `main→a→b→c uses 900 bytes total`. No existing compiler tool provides call-chain-aware stack analysis. This tool fills that gap.

## What It Does

- Analyzes LLVM bitcode (`.bc` files) at the IR level
- Computes per-function stack frame sizes from `alloca` instructions
- Constructs the static call graph using LLVM's `CallGraphWrapperPass`
- Traverses the call graph with DFS to find worst-case cumulative stack depth
- Detects recursion (cycle detection) and warns
- Flags call chains exceeding a configurable threshold
- Reports top-N deepest chains with per-function breakdown
- Web frontend with fix strategy suggestions

## Quick Start

```bash
# 1. Build the analyzer
./build.sh

# 2. Run on a test case
./run.sh testcases/test_deep_chain.c --threshold=512

# 3. Run all test cases
./run.sh --all
```

## Requirements

- LLVM 17 (Alpine: `apk add llvm17-dev llvm17-static clang17-dev`)
- CMake >= 3.20
- Ninja
- Python 3.12+ (for web frontend)
- C++17 compiler

## Manual Usage

```bash
# Compile your code to LLVM bitcode
clang -O0 -emit-llvm -c myfile.c -o myfile.bc

# Run the analyzer
./build/stack-analyzer myfile.bc --threshold=4096

# With top-N chains
./build/stack-analyzer myfile.bc --threshold=2048
```

## Web Frontend

```bash
# Start the server
python3 server.py

# Open index.html in your browser
# Drop a .bc file, set threshold, click ANALYZE
```

## Output Format

```
=== Per-Function Stack Frame Sizes ===
  c_func: 512 bytes
  b_func: 256 bytes
  a_func: 128 bytes
  main:     4 bytes

[OVERFLOW RISK] main worst-case=900 bytes (threshold=512)
  Chain: main(4B) -> a_func(128B) -> b_func(256B) -> c_func(512B)

=== Top Call Chains by Stack Depth ===
#1 900 bytes: main(4B) -> a_func(128B) -> b_func(256B) -> c_func(512B)
```

## Project Structure

```
stack-analyzer/
├── build.sh                  # Build script
├── run.sh                    # Run script
├── CMakeLists.txt            # Build configuration
├── README.md                 # This file
├── DESIGN.md                 # Design decisions and alternatives
├── IMPLEMENTATION.md         # LLVM implementation details
├── EVALUATION.md             # Metrics, test cases, comparison
├── include/
│   └── StackAnalyzer.h       # Shared header
├── lib/
│   ├── StackFrameEstimator.cpp   # Per-function frame estimation
│   └── CallChainAnalyzer.cpp     # Call graph traversal + reporting
├── tools/
│   └── stack-analyzer.cpp        # CLI entry point
├── testcases/
│   ├── test_simple.c             # Basic call chain
│   ├── test_deep_chain.c         # Deep nesting
│   ├── test_recursion.c          # Recursion detection
│   ├── test_multiple_paths.c     # Multiple call paths
│   ├── test_large_buffers.c      # Large stack allocations
│   ├── test_no_overflow.c        # Baseline - no overflow
│   └── test_freertos.sh          # FreeRTOS evaluation script
├── server.py                 # Web frontend server
└── index.html                # Web frontend UI
```

## Authors

Built as Assignment 39 — Compile-Time Stack Usage Analyzer using LLVM Pass Infrastructure.
