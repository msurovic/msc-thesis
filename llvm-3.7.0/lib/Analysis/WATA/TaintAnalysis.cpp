#include "llvm/Pass.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/ValueMap.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/raw_ostream.h"
#include <sstream>

#define PASSNAME "winapi-taint-analysis"
#define HELPTEXT "WinAPI Taint Analysis Pass"

using namespace llvm;

typedef std::pair<Value*, int>     Taint;
typedef SmallSetVector<Taint, 10>  TaintSet;
typedef ValueMap<Value*, TaintSet> TaintMap;

namespace {
  struct WinAPITaintAnalysis : public FunctionPass {
    static char ID;
    WinAPITaintAnalysis() : FunctionPass(ID) {}

    bool runOnFunction(Function&) override;
    bool runTaints(Function&, TaintMap&, TaintMap&);
    bool isTaintSource(Instruction&);
    bool isTaintSink(Instruction&);
    void printTaintGraph(TaintMap&);
  };
}

bool WinAPITaintAnalysis::runOnFunction(Function &F){
  TaintMap FunctionTaints;
  TaintMap DepGraph; 

  errs() << "Function: ";
  errs().write_escaped(F.getName());

  unsigned runs = 1;
  while(runTaints(F, FunctionTaints, DepGraph)) runs++;
  
  errs() << "\t" << runs << "\n";

  for(TaintMap::iterator i = DepGraph.begin(); i != DepGraph.end(); ++i){
    CallInst *To = cast<CallInst>(&(*i->first));
    errs() << To->getCalledFunction()->getName() << '\n';
    for(TaintSet::iterator j = i->second.begin(); j != i->second.end(); ++j){
      CallInst *From = cast<CallInst>(j->first);
      errs() << '\t' << From->getCalledFunction()->getName() << j->second << '\n';
    }
  }

  errs() << '\n';

  return false;
}

bool WinAPITaintAnalysis::runTaints(Function& F, TaintMap& FT, TaintMap& TG){
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
      Changed = IT->second.insert(Taint(&*I, -1)) || Changed;
      // Add node to dependency graph.
      if(TG.find(&*I) == TG.end()){
        Changed = TG.insert(std::make_pair(&*I, TaintSet())).second || Changed;
      }
    }
    
    if(isTaintSink(*I)){
      // Add edges to the dependency graph based on I's taints.
      TaintMap::iterator NT = TG.find(&*I);
      assert(NT != TG.end() && "Taint sink node not present in taint graph.");
      for(User::op_iterator U = I->op_begin(), UE = I->op_end(); U != UE; ++U){
        TaintMap::iterator OT = FT.find(U->get());
        if(OT != FT.end()){
          TaintSet T = OT->second;
          for(TaintSet::iterator TI = T.begin(), TE = T.end(); TI != TE; ++TI){
            Taint N(TI->first, U - I->op_begin());
            Changed = NT->second.insert(N) || Changed;
          }
        }
      }
    }else{
      // Add taints of I's operands to I's taints.
      for(User::op_iterator U = I->op_begin(), UE = I->op_end(); U != UE; ++U){
        TaintMap::iterator OT = FT.find(U->get());
        if(OT != FT.end()){
          TaintSet T = OT->second;
          for(TaintSet::iterator TI = T.begin(), TE = T.end(); TI != TE; ++TI){
            Changed = IT->second.insert(*TI) || Changed;
          }
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
  std::stringstream Nodes;
  std::stringstream Edges;

  Nodes << "# node count" << '\n';
  Nodes << "N " << TG.size() << "\n\n";
  Nodes << "# nodes declarations: V number label in_arity out_arity" << '\n';

  Edges << "# edge declarations: ";
  Edges << "E node_from:out_param_from,node_to:in_param_to" << '\n';
  
  //Print node declarations
  // unsigned NodeID = 0;
  // for(TaintGraph::iterator NI = TG.begin(), NIE = TG.end(); NI != NIE; ++NI){
  //   CallInst* NodeTo = NI->first;
  //   TaintEdgeSet NodesFrom = NI->second;

  //   Nodes << "V " << NodeID << ' ';
  //   Nodes << Node->getCalledFunction()->getName() << ' ';
  //   Nodes << Node->getNumArgOperands() << ' ';
  //   Nodes << Node->getCalledFunction()->getReturnType()->isVoidTy() ? 0 : 1;
  //   Nodes << '\n';

  //   for(TaintEdgeSet::iterator FI = Frm.begin(), FIE = Frm.end(); FI != FIE; ++FI){
  //     Edges <<  "E " << NodeID << ":0,"
  //   }
  // }
}

FunctionPass *llvm::createWinAPITaintAnalysis() {
  return new WinAPITaintAnalysis();
}

char WinAPITaintAnalysis::ID = 0;
static RegisterPass<WinAPITaintAnalysis> X(PASSNAME, HELPTEXT, false, false);