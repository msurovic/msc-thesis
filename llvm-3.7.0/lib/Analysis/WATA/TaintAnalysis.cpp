#include "llvm/Pass.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
  struct WinAPITaintAnalysis : public FunctionPass {
    static char ID;
    WinAPITaintAnalysis() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override {
      //if(F.hasDLLImportStorageClass()){
      	errs() << "WATA: ";
      	errs().write_escaped(F.getName()) << '\n';	
      //}
      return false;
    }
  };
}

FunctionPass *llvm::createWinAPITaintAnalysis() {
  return new WinAPITaintAnalysis();
}

char WinAPITaintAnalysis::ID = 0;
static RegisterPass<WinAPITaintAnalysis> X("winapi-taint-analysis", "WinAPI Taint Analysis Pass", false, false);