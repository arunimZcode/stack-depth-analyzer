#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>

using namespace llvm;

std::map<std::string, uint64_t> FrameSizeMap;

static uint64_t computeFrameSize(Function &F, const DataLayout &DL) {
  uint64_t Total = 0;
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (auto *AI = dyn_cast<AllocaInst>(&I)) {
        if (AI->isStaticAlloca()) {
          uint64_t Size = DL.getTypeAllocSize(AI->getAllocatedType());
          if (auto *C = dyn_cast<ConstantInt>(AI->getArraySize()))
            Size *= C->getZExtValue();
          Total += Size;
        }
      }
    }
  }
  return Total;
}

struct CallChainAnalyzer : public ModulePass {
  static char ID;
  uint64_t Threshold;
  CallChainAnalyzer(uint64_t T = 8192) : ModulePass(ID), Threshold(T) {}

  // Returns <worst depth, worst path> rooted at Node
  std::pair<uint64_t, std::vector<std::string>>
  dfs(CallGraphNode *Node, std::set<CallGraphNode*> &Visited) {
    Function *F = Node->getFunction();
    if (!F) return {0, {}};

    std::string Name = F->getName().str();
    uint64_t Frame = FrameSizeMap.count(Name) ? FrameSizeMap[Name] : 0;

    // Leaf case
    std::vector<std::string> BestPath = {Name + "(" + std::to_string(Frame) + "B)"};
    uint64_t BestDepth = Frame;

    for (auto &Callee : *Node) {
      if (!Callee.second) continue;
      Function *CF = Callee.second->getFunction();
      if (!CF) continue;
      if (Visited.count(Callee.second)) {
        errs() << "[RECURSION] Cycle at: " << Name << " -> "
               << CF->getName() << "\n";
        continue;
      }
      Visited.insert(Callee.second);
      auto [ChildDepth, ChildPath] = dfs(Callee.second, Visited);
      Visited.erase(Callee.second);

      uint64_t TotalDepth = Frame + ChildDepth;
      if (TotalDepth > BestDepth) {
        BestDepth = TotalDepth;
        BestPath.clear();
        BestPath.push_back(Name + "(" + std::to_string(Frame) + "B)");
        for (auto &S : ChildPath) BestPath.push_back(S);
      }
    }

    return {BestDepth, BestPath};
  }

  bool runOnModule(Module &M) override {
    const DataLayout &DL = M.getDataLayout();

    errs() << "\n=== Per-Function Stack Frame Sizes ===\n";
    for (auto &F : M) {
      if (F.isDeclaration()) continue;
      uint64_t Size = computeFrameSize(F, DL);
      FrameSizeMap[F.getName().str()] = Size;
      errs() << "  " << F.getName() << ": " << Size << " bytes\n";
    }

    CallGraph &CG = getAnalysis<CallGraphWrapperPass>().getCallGraph();
    std::vector<std::pair<uint64_t, std::string>> Results;

    for (auto &F : M) {
      if (F.isDeclaration()) continue;
      CallGraphNode *Node = CG[&F];
      std::set<CallGraphNode*> Visited;
      Visited.insert(Node);
      auto [Depth, Path] = dfs(Node, Visited);

      std::string PathStr;
      for (size_t i = 0; i < Path.size(); i++) {
        PathStr += Path[i];
        if (i + 1 < Path.size()) PathStr += " -> ";
      }
      Results.push_back({Depth, PathStr});

      if (Depth > Threshold) {
        errs() << "\n[OVERFLOW RISK] " << F.getName()
               << " worst-case=" << Depth
               << " bytes (threshold=" << Threshold << ")\n"
               << "  Chain: " << PathStr << "\n";
      }
    }

    std::sort(Results.rbegin(), Results.rend());
    errs() << "\n=== Top Call Chains by Stack Depth ===\n";
    int N = std::min((int)Results.size(), 5);
    for (int i = 0; i < N; i++) {
      errs() << "#" << (i+1) << " " << Results[i].first
             << " bytes: " << Results[i].second << "\n";
    }
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    AU.addRequired<CallGraphWrapperPass>();
  }
};

char CallChainAnalyzer::ID = 1;
static RegisterPass<CallChainAnalyzer>
  Y("call-chain", "Call-chain stack depth analyzer", false, true);

llvm::ModulePass *createCallChainAnalyzerPass(uint64_t T) {
  return new CallChainAnalyzer(T);
}
