#include "llvm/Pass.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/ADT/SetOperations.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/raw_ostream.h"

#define PASSNAME "winapi-taint-analysis"
#define HELPTEXT "WinAPI Taint Analysis Pass"

using namespace llvm;

typedef SmallPtrSet<CallInst*, 10> TaintSet;
typedef ValueMap<Value*, TaintSet> TaintMap;


typedef std::pair<CallInst*, unsigned>    TaintEdge;
typedef SmallSetVector<TaintEdge, 10>     TaintEdgeSet;
typedef ValueMap<CallInst*, TaintEdgeSet> TaintGraph;

namespace {
  struct WinAPITaintAnalysis : public FunctionPass {
    static char ID;
    WinAPITaintAnalysis() : FunctionPass(ID) {}

    bool runOnFunction(Function&) override;
    bool runTaints(Function&, TaintMap&, TaintGraph&);
    bool isTaintSource(Instruction&);
    bool isTaintSink(Instruction&);
    void printTaintGraph(TaintMap&);
  };
}

bool WinAPITaintAnalysis::runOnFunction(Function &F){
  TaintMap   FTaints;
  TaintGraph FGraph; 

  errs() << "Function: ";
  errs().write_escaped(F.getName());

  unsigned runs = 1;
  while(runTaints(F, FTaints, FGraph)) runs++;
  
  errs() << "\t" << runs << "\n";

  // for(TaintMap::iterator i = FunctionTaints.begin(); i != FunctionTaints.end(); ++i){
  //   if(i->second.size() > 0)
  //     errs() << *i->first << '\t' << i->second.size() << '\n';
  // }

  for(TaintGraph::iterator i = FGraph.begin(); i != FGraph.end(); ++i){
    const CallInst *CI = cast<CallInst>(&(*i->first));
    errs() << CI->getCalledFunction()->getName() << '\n';
    for(TaintEdgeSet::iterator j = i->second.begin(); j != i->second.end(); ++j)
      errs() << '\t' << ((*j).first)->getCalledFunction()->getName() << ((*j).second) << '\n';
  }

  errs() << '\n';

  return false;
}

bool WinAPITaintAnalysis::runTaints(Function& F, TaintMap& FT, TaintGraph& TG){
  bool Changed = false;

  for(inst_iterator I = inst_begin(F), IE = inst_end(F); I != IE; ++I){
    // Add entry into Taints for I.
    TaintMap::iterator IT = FT.find(&*I);
    if(IT == FT.end()){
      IT = FT.insert(std::make_pair(&*I, TaintSet())).first;
      Changed = true;
    }

    if(isTaintSource(*I)){
      // Add I itself to I's taints.
      CallInst *CI = cast<CallInst>(&*I);
      Changed = (IT->second.insert(CI)).second || Changed;
      // Add node to dependency graph.
      if(TG.find(CI) == TG.end()){
        Changed = TG.insert(std::make_pair(CI, TaintEdgeSet())).second || Changed;
      }
    }
    
    if(isTaintSink(*I)){
      // Add edges to the dependency graph based on I's taints.
      CallInst *CI = cast<CallInst>(&*I);
      TaintGraph::iterator GN = TG.find(CI);
      assert(GN != TG.end() && "Taint sink node not present in taint graph.");
      for(User::op_iterator U = I->op_begin(), UE = I->op_end(); U != UE; ++U){
        if(FT.find(U->get()) != FT.end()){ 
          TaintSet UT = FT.find(U->get())->second;
          for(TaintSet::iterator UTI = UT.begin(), UTE = UT.end(); UTI != UTE; ++UTI){
            Changed = GN->second.insert(std::make_pair(*UTI, U - I->op_begin())) || Changed;
          }
        }
      }
    }else{
      // Add taints of I's operands to I's taints.
      for(User::op_iterator U = I->op_begin(), UE = I->op_end(); U != UE; ++U){
        TaintMap::iterator UT = FT.find(U->get());
        if(UT != FT.end()){
          Changed = set_union(IT->second, UT->second) || Changed;
        }
      }
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

void WinAPITaintAnalysis::printTaintGraph(TaintMap& TG){
  // errs() << "# node count" << '\n';
  // errs() << "N " << TG.size() << "\n\n";
  
  // // Print node declarations
  // errs() << "# nodes declarations: V number label in_arity out_arity" << '\n';
  // unsigned NodeID = 0;
  // for(TaintMap::iterator NI = TG.begin(), NIE = TG.end(); NI != NIE; ++NI){
  //   CallInst* Node = cast<CallInst>(&*NI->first);
  //   errs() << "V " << NodeID << ' ';
  //   errs() << Node->getCalledFunction()->getName() << ' ';
  //   errs() << Node->getNumArgOperands() << ' ';
  //   errs() << Node->getCalledFunction()->getReturnType()->isVoidTy() ? 0 : 1;
  //   errs() << '\n';
  // }
  
  // // Print edge declarations
  // errs() << "# edge declarations: ";
  // errs() << "E node_from:out_param_from,node_to:in_param_to" << '\n';
  // for(TaintMap::iterator NI = TG.begin(), NIE = TG.end(); NI != NIE; ++NI){
  //   CallInst* To  = cast<CallInst>(&*NI->first);
  //   TaintSet  Frm = NI->second;
  //   for(TainSet::iterator FI = Frm.begin(), FIE = Frm.end(); FI != FIE; ++FI){
      
  //   }
  // }
}

FunctionPass *llvm::createWinAPITaintAnalysis() {
  return new WinAPITaintAnalysis();
}

char WinAPITaintAnalysis::ID = 0;
static RegisterPass<WinAPITaintAnalysis> X(PASSNAME, HELPTEXT, false, false);