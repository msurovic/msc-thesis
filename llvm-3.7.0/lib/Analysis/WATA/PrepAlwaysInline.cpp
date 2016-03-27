#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/raw_ostream.h"

#define PASSNAME "prep-always-inline"
#define HELPTEXT "Prepare functions for always_inline"

using namespace llvm;

namespace {
  struct PrepAlwaysInline : public ModulePass {
    static char ID;
    PrepAlwaysInline() : ModulePass(ID) {}

    bool runOnModule(Module&) override;
  };
}

bool PrepAlwaysInline::runOnModule(Module& M){
  for(Function& F : M){
    if(!F.empty()){
      bool isSignificant = false;

      for(BasicBlock& BB : F){
        for(Instruction& I : BB){
          if(CallInst* CI = dyn_cast<CallInst>(&I)){
            if(Function* CF = dyn_cast<Function>(CI->getCalledValue())){
              isSignificant = CF->hasDLLImportStorageClass() || isSignificant;
            }
          }
        }
      }

      if(F.hasFnAttribute(Attribute::AlwaysInline)){
        F.removeFnAttr(Attribute::AlwaysInline);
      }
      if(F.hasFnAttribute(Attribute::NoInline)){
        F.removeFnAttr(Attribute::NoInline);
      }
      if(F.getNumUses() > 0 && isSignificant){
        F.addFnAttr(Attribute::AlwaysInline);
      }
    }
  }
  return true;
}

ModulePass* llvm::createPrepAlwaysInline() {
  return new PrepAlwaysInline();
}

char PrepAlwaysInline::ID = 0;
static RegisterPass<PrepAlwaysInline> X(PASSNAME, HELPTEXT, false, false);