# Implementation Document — Stack Depth Analyzer

## LLVM Pass Infrastructure

This tool is built on LLVM's legacy pass infrastructure (compatible with LLVM 17), using two passes that run sequentially via `legacy::PassManager`.

---

## Pass 1: StackFrameEstimator (MachineFunctionPass)

**File:** `lib/StackFrameEstimator.cpp`  
**Base class:** `MachineFunctionPass`  
**Pass ID:** `stack-frame-est`

### What it does
Estimates per-function stack frame size by walking `alloca` instructions in the LLVM IR. Each `alloca` represents a stack-allocated variable.

### Key LLVM APIs used

```cpp
// Walk every instruction in every basic block
for (auto &BB : F) {
  for (auto &I : BB) {
    if (auto *AI = dyn_cast<AllocaInst>(&I)) {
      // AllocaInst = stack allocation
      uint64_t Size = DL.getTypeAllocSize(AI->getAllocatedType());
      Total += Size;
    }
  }
}
```

| API | Purpose |
|---|---|
| `AllocaInst` | Represents a stack allocation (`char buf[256]` → `alloca [256 x i8]`) |
| `DataLayout::getTypeAllocSize()` | Returns the size in bytes of a type, with alignment padding |
| `ConstantInt` | Checks if the array size is a compile-time constant (static alloca) |
| `isStaticAlloca()` | Returns true if the allocation size is known at compile time |

### Why alloca?
Every local variable in C that the compiler puts on the stack becomes an `alloca` in LLVM IR. For example:

```c
void b_func() {
    char buf[256];  // →  %buf = alloca [256 x i8]
    int x = 5;      // →  %x = alloca i32
}
```

The `DataLayout` object knows the exact byte size of every LLVM type, including struct padding and alignment requirements.

---

## Pass 2: CallChainAnalyzer (ModulePass)

**File:** `lib/CallChainAnalyzer.cpp`  
**Base class:** `ModulePass`  
**Pass ID:** `call-chain`

### What it does
1. Builds `FrameSizeMap` from IR alloca instructions
2. Requests LLVM's call graph via `CallGraphWrapperPass`
3. Runs DFS from each entry point to find worst-case cumulative depth
4. Reports results and threshold violations

### Key LLVM APIs used

```cpp
// Request the call graph (declared as a dependency)
void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<CallGraphWrapperPass>();
}

// Get the call graph in runOnModule
CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
```

| API | Purpose |
|---|---|
| `CallGraph` | Whole-program call graph — maps Function* → CallGraphNode* |
| `CallGraphNode` | One node per function; iterating gives its callees |
| `CallGraphWrapperPass` | LLVM analysis pass that builds the CallGraph |
| `ModulePass::runOnModule()` | Entry point — sees entire Module at once |
| `Module::getDataLayout()` | Gets target data layout for size calculations |
| `AU.addRequired<>()` | Declares pass dependency — ensures CallGraph is built first |

### DFS Algorithm

```cpp
std::pair<uint64_t, std::vector<std::string>>
dfs(CallGraphNode *Node, std::set<CallGraphNode*> &Visited) {
    Function *F = Node->getFunction();
    uint64_t Frame = FrameSizeMap[F->getName().str()];
    
    // Base case: leaf node
    uint64_t BestDepth = Frame;
    std::vector<std::string> BestPath = {F->getName() + "(" + Frame + "B)"};
    
    // Recurse into callees
    for (auto &Callee : *Node) {
        if (Visited.count(Callee.second)) {
            // CYCLE DETECTED — recursion
            continue;
        }
        Visited.insert(Callee.second);
        auto [ChildDepth, ChildPath] = dfs(Callee.second, Visited);
        Visited.erase(Callee.second);
        
        // Keep worst (deepest) path
        if (Frame + ChildDepth > BestDepth) {
            BestDepth = Frame + ChildDepth;
            BestPath = {current} + ChildPath;
        }
    }
    return {BestDepth, BestPath};
}
```

**Time complexity:** O(V + E) per entry point, where V = functions, E = call edges  
**Space complexity:** O(depth) for the recursion stack + O(V) for the visited set

---

## CLI Tool

**File:** `tools/stack-analyzer.cpp`

Uses LLVM's `CommandLine` library for argument parsing:

```cpp
cl::opt<std::string> InputFile(cl::Positional, cl::desc("<input.bc>"));
cl::opt<uint64_t> Threshold("threshold", cl::init(8192));
```

PassManager setup:
```cpp
legacy::PassManager PM;
PM.add(new CallGraphWrapperPass());   // dependency
PM.add(createCallChainAnalyzerPass(Threshold));  // our pass
PM.run(*M);
```

---

## Build System

Uses CMake with `find_package(LLVM)` to locate LLVM headers and libraries:

```cmake
find_package(LLVM REQUIRED CONFIG
  PATHS /usr/lib/llvm17/lib/cmake/llvm)

llvm_map_components_to_libnames(LLVM_LIBS
  core support irreader analysis codegen passes ...)
```

The pass is built as a static library (`libStackAnalyzerPasses.a`) and linked into the CLI tool.

---

## Data Flow

```
parseIRFile(input.bc)
    │
    └──► LLVM Module (in memory)
              │
              ├── for each Function F:
              │       for each BasicBlock BB in F:
              │           for each Instruction I in BB:
              │               if AllocaInst: add size to FrameSizeMap[F]
              │
              ├── CallGraphWrapperPass builds CallGraph
              │
              └── for each Function F (entry points):
                      DFS(CallGraph[F], visited={})
                          → returns (worst_depth, worst_path)
                      if worst_depth > threshold:
                          print [OVERFLOW RISK]
                      collect all (depth, path) pairs
                      sort descending
                      print top-5
```

---

## LLVM IR Example

Given this C code:
```c
void c_func() { char buf[512]; }
void b_func() { char buf[256]; c_func(); }
void a_func() { char buf[128]; b_func(); }
int  main()   { int x; a_func(); return 0; }
```

LLVM IR (simplified):
```llvm
define void @c_func() {
  %buf = alloca [512 x i8]    ; 512 bytes
  ret void
}
define void @b_func() {
  %buf = alloca [256 x i8]    ; 256 bytes
  call void @c_func()
  ret void
}
define void @a_func() {
  %buf = alloca [128 x i8]    ; 128 bytes
  call void @b_func()
  ret void
}
define i32 @main() {
  %x = alloca i32             ; 4 bytes
  call void @a_func()
  ret i32 0
}
```

Our pass reads the alloca sizes → builds call graph → DFS gives:
```
main(4) → a_func(128) → b_func(256) → c_func(512) = 900 bytes
```
