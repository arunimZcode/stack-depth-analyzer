#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/CallGraph.h"

using namespace llvm;

// Forward declare pass registration
namespace llvm {
  void initializeCallGraphWrapperPassPass(PassRegistry &);
}

cl::opt<std::string> InputFile(cl::Positional,
  cl::desc("<input.bc or input.ll>"), cl::Required);
cl::opt<uint64_t> Threshold("threshold",
  cl::desc("Overflow warning threshold in bytes"), cl::init(8192));

// Declared in our pass files
extern llvm::FunctionPass *createStackFrameEstimatorPass();
extern llvm::ModulePass *createCallChainAnalyzerPass(uint64_t T);

int main(int argc, char **argv) {
  cl::ParseCommandLineOptions(argc, argv, "Stack Usage Analyzer\n");

  LLVMContext Ctx;
  SMDiagnostic Err;
  auto M = parseIRFile(InputFile, Err, Ctx);
  if (!M) {
    Err.print(argv[0], errs());
    return 1;
  }

  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  initializeCallGraphWrapperPassPass(Registry);

  legacy::PassManager PM;
  PM.add(new CallGraphWrapperPass());
  PM.add(createCallChainAnalyzerPass(Threshold));
  PM.run(*M);

  return 0;
}
