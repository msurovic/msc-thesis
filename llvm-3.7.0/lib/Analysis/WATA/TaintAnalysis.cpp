#include "llvm/Pass.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/ADT/SetOperations.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/raw_ostream.h"

#define PASSNAME "winapi-taint-analysis"
#define HELPTEXT "WinAPI Taint Analysis Pass"

using namespace llvm;

typedef SmallPtrSet<const CallInst*, 20> TaintSet;
typedef ValueMap<const Value*, TaintSet> TaintMap;

namespace {
  struct WinAPITaintAnalysis : public FunctionPass {
    static char ID;
    WinAPITaintAnalysis() : FunctionPass(ID) {}

    bool runOnFunction(Function&) override;
    bool runTaints(Function&, TaintMap&);
    bool isTaintSource(Instruction&);
    bool isTaintSink(Instruction&);
  };
}

bool WinAPITaintAnalysis::runOnFunction(Function &F){
  TaintMap FunctionTaints;

  errs() << "Function: ";
  errs().write_escaped(F.getName());

  unsigned runs = 1;
  while(runTaints(F, FunctionTaints)) runs++;
  
  errs() << "\t\t" << runs << "\n\n";

  for(TaintMap::iterator i = FunctionTaints.begin(); i != FunctionTaints.end(); ++i){
    if(i->second.size() > 0)
      errs() << *i->first << '\t' << i->second.size() << '\n';
  }

  return false;
}

bool WinAPITaintAnalysis::runTaints(Function& F, TaintMap& FT){
  bool Changed = false;

  for(inst_iterator I = inst_begin(F), IE = inst_end(F); I != IE; ++I){
    // Add entry into Taints for I.
    TaintMap::iterator IT = FT.find(&*I);
    if(IT == FT.end()){
      IT = FT.insert(std::make_pair(&*I, TaintSet())).first;
    }

    // Add taints of I's operands to I's taints.
    for(User::op_iterator U = I->op_begin(), UE = I->op_end(); U != UE; ++U){
      TaintMap::iterator UT = FT.find(U->get());
      if(UT != FT.end()){
        Changed = set_union(IT->second, UT->second) || Changed;
      }
    }

    if(isTaintSource(*I)){
      // Add I itself to I's taints.
      CallInst *CI = cast<CallInst>(&*I);
      Changed = (IT->second.insert(CI)).second || Changed;
      // Add node to dependency graph.
      
      // CallInst *CI = &cast<CallInst>(*I);
      // errs() << "\t Source: ";
      // errs().write_escaped(CI->getCalledFunction()->getName()) << '\n';
    }

    if(isTaintSink(*I)){
      // Add edges to the dependency graph based on I's taints.

      // CallInst *CI = &cast<CallInst>(*I);
      // errs() << "\t Sink: ";
      // errs().write_escaped(CI->getCalledFunction()->getName()) << "\n\n";
    }
  }
  
  return Changed;
}

bool WinAPITaintAnalysis::isTaintSource(Instruction& I){
  if(CallInst *CI = dyn_cast<CallInst>(&I)){
    return CI->getCalledFunction()->hasDLLImportStorageClass();
  }else{
    return false;
  }
}

bool WinAPITaintAnalysis::isTaintSink(Instruction& I){
  return isTaintSource(I);
}

FunctionPass *llvm::createWinAPITaintAnalysis() {
  return new WinAPITaintAnalysis();
}

char WinAPITaintAnalysis::ID = 0;
static RegisterPass<WinAPITaintAnalysis> X(PASSNAME, HELPTEXT, false, false);