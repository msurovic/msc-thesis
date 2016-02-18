#include "llvm/Pass.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include <sstream>
#include <system_error>

#define PASSNAME "winapi-taint-analysis"
#define HELPTEXT "WinAPI Taint Analysis Pass"

using namespace llvm;

typedef std::pair<Value*, int>      Taint;
typedef SmallSetVector<Taint, 20>   TaintSet;
typedef MapVector<Value*, TaintSet> TaintMap;

namespace {
  struct WinAPITaintAnalysis : public ModulePass {
    static char ID;
    WinAPITaintAnalysis() : ModulePass(ID) {}

    bool runOnModule(Module&) override;
    bool runTaints(Function&, TaintMap&, TaintMap&);
    bool makeTaints(Instruction&, TaintMap&);
    bool propTaints(Instruction&, TaintMap&);
    bool sinkTaints(Instruction&, TaintMap&, TaintMap&);
    bool isTaintSource(Instruction&);
    bool isTaintSink(Instruction&);
    bool taintSetUnion(TaintSet&, TaintSet&);
    void finalizeTaintGraph(TaintMap&, TaintMap&);
    void printTaintGraph(TaintMap&, Module&);
  };
}

bool WinAPITaintAnalysis::runOnModule(Module &M){
  TaintMap DepGraph;
  TaintMap ModuleTaints;

  for(Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI){
    runTaints(*MI, ModuleTaints, DepGraph);
  }

  finalizeTaintGraph(DepGraph, ModuleTaints);
  printTaintGraph(DepGraph, M);

  return false;
}

bool WinAPITaintAnalysis::runTaints(Function& F, TaintMap& MT, TaintMap& TG){
  bool Changed = false;
  return Changed;
}

bool WinAPITaintAnalysis::isTaintSource(Instruction& I){
  if(CallInst* CI = dyn_cast<CallInst>(&I)){
    if(Function* F = dyn_cast<Function>(CI->getCalledValue())){
      return F->hasDLLImportStorageClass();
    }
  }
  return false;
}

bool WinAPITaintAnalysis::isTaintSink(Instruction& I){
  return isTaintSource(I);
}

bool WinAPITaintAnalysis::taintSetUnion(TaintSet& A, TaintSet& B){
  bool Changed = false;
  for(TaintSet::iterator TI = B.begin(), TE = B.end(); TI != TE; ++TI){
    Changed = A.insert(*TI) || Changed;
  }
  return Changed;
}

bool WinAPITaintAnalysis::makeTaints(Instruction& I, TaintMap& MT){
  bool Changed = false;
  // Find I's taints in MT. If I does not have a record in MT, create it.
  TaintMap::iterator IT = MT.find(&I);
  if(IT == MT.end()){
    IT = MT.insert(std::make_pair(&I, TaintSet())).first;
    Changed = true;
  }
  // Add I itself to I's taints.
  Changed = IT->second.insert(Taint(&I, -1)) || Changed;
  // Any pointer type variable operand of I is tainted by I.
  for(Value* V : I.operand_values()){
    if(!isa<Constant>(V) && V->getType()->isPtrOrPtrVectorTy()){
      TaintMap::iterator OT = MT.find(V);
      if(OT == MT.end()){
        OT = MT.insert(std::make_pair(&I, TaintSet())).first;
        Changed = true;
      }
      Changed = OT->second.insert(Taint(&I, -1)) || Changed;
    }
  }
  return Changed;
}

bool WinAPITaintAnalysis::propTaints(Instruction& I, TaintMap& MT){
  bool Changed = false;
  // Find I's taints in MT. If I does not have a record in MT, create it.
  TaintMap::iterator IT = MT.find(&I);
  if(IT == MT.end()){
    IT = MT.insert(std::make_pair(&I, TaintSet())).first;
    Changed = true;
  }
  // Add taints of I's operands into I's taints. Constants do not have
  // taints.
  for(Value* V : I.operand_values()){
    if(!isa<Constant>(V)){
      TaintMap::iterator OT = MT.find(V);
      if(OT != MT.end() && !OT->second.empty()){
        Changed = taintSetUnion(IT->second, OT->second) || Changed;
      }
    }
  }
  return Changed;
}

bool WinAPITaintAnalysis::sinkTaints(Instruction& I, TaintMap& MT, TaintMap& TG){
  bool Changed = false;
  // Add taint sink node into the dependency graph.
  TaintMap::iterator DN = TG.find(&I);
  if(DN == TG.end()){
    DN = TG.insert(std::make_pair(&I, TaintSet())).first;
    Changed = true;
  }
  // Iterate through the sink's operands and sink their taint if they have any.
  for(User::op_iterator U = I->op_begin(), UE = I->op_end(); U != UE; ++U){
    Value* V = U->get();
    unsigned OpIdx = U - I->op_begin();
    TaintMap::iterator OT = MT.find(V);
    if(OT != MT.end() && !OT->second.empty()){
      TaintSet T = OT->second;
      for(TaintSet::iterator TI = T.begin(), TE = T.end(); TI != TE; ++TI){
        // Create taint source node if there isn't one.
        if(TG.find(TI->first) == TG.end()){
          TG.insert(std::make_pair(TI->first, TaintSet()));
          Changed = true;
        }
        // Create the edge from taint source TI->first into DN taint sink's
        // operand on index OpIdx.
        Changed = DN->second.insert(Taint(TI->first, OpIdx)) || Changed;
      }
      // If V is a pointer type variable I sinks all it's current taints.
      // Note: sinkTaints should probably only report changes of TG. The
      // clearing of T is done due to the stateful nature of pointer
      // variables.
      if(!isa<Constant>(V) && V->getType()->isPtrOrPtrVectorTy()){
        T.clear(); 
        //Changed = true;
      }
    }
  }
  return Changed;
}

void WinAPITaintAnalysis::finalizeTaintGraph(TaintMap& TG, TaintMap& MT){
  // Operands that do not have an entry in MT, so a terminal node needs to be
  // created. The node will be labeled by the type of the operand. If I is
  // a CallInst we should skip handling the called Value.
  for(TaintMap::iterator NI = TG.begin(), NIE = TG.end(); NI != NIE; ++NI){
    if(Instruction* I = dyn_cast<Instruction>(NI->first)){
      TaintMap::iterator DN = TG.find(I);
      for(User::op_iterator U = I->op_begin(), UE = I->op_end(); U != UE; ++U){
        Value* V = U->get();
        unsigned OpIdx = U - I->op_begin();
        TaintMap::iterator OT = MT.find(V);
        if(OT == MT.end() || OT->second.empty()){
          if(CallInst *CI = dyn_cast<CallInst>(I)){
            if(V == CI->getCalledValue()){
              continue;
            }
          }
          if(TG.find(V) == TG.end()){
            TG.insert(std::make_pair(V, TaintSet()));
            DN = TG.find(I);
          }
          DN->second.insert(Taint(V, OpIdx));
        }
      }
    }else{
      assert(false && "Taint graph node is not an Instruction.");
    }
  }
}

void WinAPITaintAnalysis::printTaintGraph(TaintMap& TG, Module& M){
  std::stringstream Nodes;
  std::stringstream Edges;

  Nodes << "# node count" << '\n';
  Nodes << "N " << TG.size() << "\n\n";
  Nodes << "# nodes declarations: V number label in_arity out_arity" << '\n';

  Edges << "# edge declarations: ";
  Edges << "E node_from:out_param_from,node_to:in_param_to" << '\n';
  
  for(TaintMap::iterator DI = TG.begin(), DIE = TG.end(); DI != DIE; ++DI){
    TaintSet Src = DI->second;
    unsigned DstID = DI - TG.begin();

    Nodes << "V " << DstID << ' ';

    if(CallInst* Dst = dyn_cast<CallInst>(DI->first)){
      Nodes << Dst->getCalledFunction()->getName().str() << ' ';
      Nodes << Dst->getNumArgOperands() << ' ';
      Nodes << (Dst->getCalledFunction()->getReturnType()->isVoidTy() ? 0 : 1);
      Nodes << "\n";

      for(TaintSet::iterator SI = Src.begin(), SIE = Src.end(); SI != SIE; ++SI){
        unsigned SrcID = TG.find(SI->first) - TG.begin();
        Edges <<  "E " << SrcID << ":0," << DstID << ':' << SI->second << '\n';
      }
    }else{
      std::string ConstType;
      raw_string_ostream rso(ConstType);
      if(Function* F = dyn_cast<Function>(DI->first)){
        F->getReturnType()->print(rso);
      }else{
        DI->first->getType()->print(rso);
      }
      Nodes << rso.str() << ' ' << "0 1" << '\n';
    }
  }

  std::error_code EC;
  std::string OutputFileName = M.getName().str() + ".sdg";
  raw_fd_ostream OutputFile(OutputFileName, EC, sys::fs::F_Text);

  if(EC){
    errs() << M.getName() << ": error opening " << OutputFileName << ":"
           << EC.message() << "\n";
  }else{
    OutputFile << Nodes.str() << '\n' << Edges.str() << '\n';
    OutputFile.close();
  }
}

ModulePass *llvm::createWinAPITaintAnalysis() {
  return new WinAPITaintAnalysis();
}

char WinAPITaintAnalysis::ID = 0;
static RegisterPass<WinAPITaintAnalysis> X(PASSNAME, HELPTEXT, false, false);