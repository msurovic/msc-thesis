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
    void finalizeTaintGraph(TaintMap&, TaintMap&);
    bool taintSetUnion(TaintSet&, TaintSet&);
    bool isTaintSource(Instruction&);
    bool isTaintSink(Instruction&);
    void printTaintGraph(TaintMap&, Module&);
  };
}

bool WinAPITaintAnalysis::runOnModule(Module &M){
  TaintMap DepGraph;
  TaintMap ModuleTaints;

  bool Changed = true;

  while(Changed){
    Changed = false;
    for(Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI){
      if(!MI->empty()){
        while(runTaints(*MI, ModuleTaints, DepGraph)){
          Changed = true;
        }
      }
    }
  }

  finalizeTaintGraph(DepGraph, ModuleTaints);
  printTaintGraph(DepGraph, M);

  return false;
}

bool WinAPITaintAnalysis::runTaints(Function& F, TaintMap& MT, TaintMap& TG){
  bool Changed = false;

  // Run the taints
  for(inst_iterator I = inst_begin(F), IE = inst_end(F); I != IE; ++I){
    // Add entry into MT for I.
    TaintMap::iterator IT = MT.find(&*I);
    if(IT == MT.end()){
      IT = MT.insert(std::make_pair(&*I, TaintSet())).first;
      Changed = true;
    }

    if(isTaintSource(*I)){
      // Add I itself to I's taints.
      Changed = IT->second.insert(Taint(&*I, -1)) || Changed;
      // Any pointer type variable operand of I is tainted by I.
      // Note: This behaves weirdly. Fix it sometime.
      // for(Value* V : I->operand_values()){
      //   if(!isa<Constant>(V) && V->getType()->isPtrOrPtrVectorTy()){
      //     // Add entry into MT for V.
      //     TaintMap::iterator VT = MT.find(V);
      //     if(VT == MT.end()){
      //       VT = MT.insert(std::make_pair(V, TaintSet())).first;
      //       Changed = true;
      //     }
      //     // Add I into V's taints.
      //     Changed = VT->second.insert(Taint(&*I, -1)) || Changed;
      //   }
      // }
      // Add node to dependency graph.
      if(TG.find(&*I) == TG.end()){
        Changed = TG.insert(std::make_pair(&*I, TaintSet())).second || Changed;
      }
    }

    for(User::op_iterator U = I->op_begin(), UE = I->op_end(); U != UE; ++U){
      Value* V = U->get();
      TaintMap::iterator OT = MT.find(V);
      unsigned OpIdx = U - I->op_begin();

      if(isTaintSink(*I)){
        // Add edges to the dependency graph based on operands taints.
        TaintMap::iterator NT = TG.find(&*I);
        assert(NT != TG.end() && "Taint sink node not present in taint graph.");
        if(OT != MT.end() && !OT->second.empty()){
          // Operand is a Value* that has an entry in MT. Use taints in the entry.
          TaintSet T = OT->second;
          for(TaintSet::iterator TI = T.begin(), TE = T.end(); TI != TE; ++TI){
            // If *TI is the same as *NT we do not sink it. Taint sinks cannot
            // sink their own taints.
            if(NT->first != TI->first){
              Changed = NT->second.insert(Taint(TI->first, OpIdx)) || Changed;
            }
          }
        }
      }else{
        if(OT != MT.end()){
          if(CallInst *CI = dyn_cast<CallInst>(&*I)){
            if(Function* CF = dyn_cast<Function>(CI->getCalledValue())){
              // If I is a CallInst to a function CF with a definition in
              // module M, taint CF's arguments.
              if(!CF->empty()){
                Function::arg_iterator AI = CF->arg_begin();
                for(unsigned i = 0; i < OpIdx; i++) AI++;
                TaintMap::iterator AT = MT.find(&*AI);
                if(AT == MT.end()){
                  AT = MT.insert(std::make_pair(&*AI, TaintSet())).first;
                  Changed = true;
                }
                Changed = taintSetUnion(AT->second, OT->second) || Changed;
              }
            }
          }else if(ReturnInst *RI = dyn_cast<ReturnInst>(&*I)){
            // If I is a ReturnInst we need to taint all the CallInsts that use
            // Function F.
            for(User* FU : F.users()){
              if(isa<Instruction>(FU)){
                TaintMap::iterator FUT = MT.find(FU);
                if(FUT == MT.end()){
                  FUT = MT.insert(std::make_pair(FU, TaintSet())).first;
                  Changed = true;
                }
                Changed = taintSetUnion(FUT->second, OT->second) || Changed;
              }
            }
          }
          // Add taints of I's operands to I's taints.
          Changed = taintSetUnion(IT->second, OT->second) || Changed;
        }
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
      TaintMap::iterator NT = TG.find(I);
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
            NT = TG.find(I);
          }
          NT->second.insert(Taint(V, OpIdx));
        }
      }
    }else{
      assert(false && "Taint graph node is not an Instruction.");
    }
  }
}

bool WinAPITaintAnalysis::taintSetUnion(TaintSet& A, TaintSet& B){
  bool Changed = false;
  for(TaintSet::iterator TI = B.begin(), TE = B.end(); TI != TE; ++TI){
    Changed = A.insert(*TI) || Changed;
  }
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