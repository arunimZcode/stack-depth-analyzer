#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <string>

using namespace llvm;

// Defined in CallChainAnalyzer.cpp
extern std::map<std::string, uint64_t> FrameSizeMap;

struct StackFrameEstimator : public MachineFunctionPass {
  static char ID;
  StackFrameEstimator() : MachineFunctionPass(ID) {}

  bool runOnMachineFunction(MachineFunction &MF) override {
    const MachineFrameInfo &MFI = MF.getFrameInfo();
    uint64_t FrameSize = MFI.estimateStackSize(MF);
    FrameSizeMap[MF.getName().str()] = FrameSize;
    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

char StackFrameEstimator::ID = 0;
static RegisterPass<StackFrameEstimator>
  X("stack-frame-est", "Per-function stack frame estimator", false, true);
