#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include <iterator>
#include <sstream>
#include <system_error>

#define PASSNAME "winapi-taint-analysis"
#define HELPTEXT "WinAPI Taint Analysis Pass"

using namespace llvm;

typedef std::pair<Value*, int>      Taint;
typedef SmallSetVector<Taint, 5>    TaintSet;
typedef MapVector<Value*, TaintSet> TaintMap;

namespace {
  struct WinAPITaintAnalysis : public ModulePass {
    static char ID;
    WinAPITaintAnalysis() : ModulePass(ID) {}

    bool runOnModule(Module&) override;
    void runTaints(Function&, TaintMap&);
    bool makeTaints(Instruction&, TaintMap&);
    bool propTaints(Instruction&, TaintMap&);
    bool sinkTaints(Instruction&, TaintMap&, TaintMap&);
    bool isTaintSource(Instruction&);
    bool isTaintSink(Instruction&);
    bool taintSetUnion(TaintSet&, TaintSet&);
    bool taintMapUnion(TaintMap&, TaintMap&);
    bool taintMapEquiv(TaintMap&, TaintMap&);
    void unfoldTaintGraph(TaintMap&);
    void finalizeTaintGraph(TaintMap&);
    void printTaintGraph(TaintMap&, Module&);
  };
}

bool WinAPITaintAnalysis::runOnModule(Module& M){
  TaintMap DepGraph;

  //errs() << "Checkpoint 1" << '\n';

  for(Function& F : M){
    runTaints(F, DepGraph);
  }

  //errs() << "Checkpoint 2" << '\n';
  unfoldTaintGraph(DepGraph);
  //errs() << "Checkpoint 3" << '\n';
  //finalizeTaintGraph(DepGraph);
  //errs() << "Checkpoint 4" << '\n';
  //printTaintGraph(DepGraph, M);
  //errs() << "Checkpoint 5" << '\n';

  return false;
}

void WinAPITaintAnalysis::runTaints(Function& F, TaintMap& TG){
  MapVector<BasicBlock*, TaintMap> AbsContexts;
  bool Changed = true;
  // Iteratively compute abstract contexts until a fixpoint is reached.
  // The assumption here is that we have all of the functions inlined already.
  while(Changed){
    Changed = false;
    for(BasicBlock& BB : F){
      // Create tha abstract context for BB.
      TaintMap BBT;
      // Join with all predecessor contexts.
      for(BasicBlock* P : predecessors(&BB)){
        MapVector<BasicBlock*, TaintMap>::iterator PT = AbsContexts.find(P);
        if(PT == AbsContexts.end()){
          PT = AbsContexts.insert(std::make_pair(P, TaintMap())).first;
          Changed = true;
        }
        taintMapUnion(BBT, PT->second);
      }
      // Analyze BB
      for(Instruction& I : BB){
        if(isTaintSink(I)){
          Changed = sinkTaints(I, BBT, TG) || Changed;
        }
        if(isTaintSource(I)){
          makeTaints(I, BBT);
        }else{
          propTaints(I, BBT);
        }
      }
      // Determine if there was a change in the abstract context and update
      // the abstract context if there was.
      MapVector<BasicBlock*, TaintMap>::iterator CI = AbsContexts.find(&BB);
      if(CI == AbsContexts.end()){
        AbsContexts.insert(std::make_pair(&BB, BBT));
        Changed = true;
      }else if(!taintMapEquiv(CI->second, BBT)){
        CI->second = BBT;
        Changed = true;
      }
    }
  }
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

bool WinAPITaintAnalysis::taintMapUnion(TaintMap& A, TaintMap& B){
  bool Changed = false;
  for(TaintMap::iterator BI = B.begin(), BE = B.end(); BI != BE; ++BI){
    TaintMap::iterator AI = A.find(BI->first);    
    if(AI == A.end()){
      AI = A.insert(std::make_pair(BI->first, TaintSet())).first;
      Changed = true;
    }
    Changed = taintSetUnion(AI->second, BI->second) || Changed;
  }
  return Changed;
}

bool WinAPITaintAnalysis::taintMapEquiv(TaintMap& A, TaintMap& B){
  if(A.size() == B.size()){
    for(TaintMap::iterator BI = B.begin(), BE = B.end(); BI != BE; ++BI){
      TaintMap::iterator AI = A.find(BI->first);
      if(AI != A.end()){
        if(AI->second != BI->second){
          return false;
        }
      }else{
        return false; 
      }
    }
  }else{
    return false;
  }
  return true;
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

bool WinAPITaintAnalysis::propTaints(Instruction& I, TaintMap& TM){
  bool Changed = false;
  // Find I's taints in TM. If I does not have a record in TM, create it.
  TaintMap::iterator IT = TM.find(&I);
  if(IT == TM.end()){
    IT = TM.insert(std::make_pair(&I, TaintSet())).first;
    Changed = true;
  }
  // Add taints of I's operands into I's taints. Constants do not have
  // taints.
  for(Value* V : I.operand_values()){
    if(!isa<Constant>(V)){
      TaintMap::iterator OT = TM.find(V);
      if(OT != TM.end() && !OT->second.empty()){
        Changed = taintSetUnion(IT->second, OT->second) || Changed;
      }
    }
  }
  return Changed;
}

bool WinAPITaintAnalysis::sinkTaints(Instruction& I, TaintMap& TM, TaintMap& TG){
  bool Changed = false;
  // Add taint sink node into the dependency graph.
  TaintMap::iterator DN = TG.find(&I);
  if(DN == TG.end()){
    DN = TG.insert(std::make_pair(&I, TaintSet())).first;
    Changed = true;
  }
  // Iterate through the sink's operands and sink their taint if they have any.
  for(User::op_iterator U = I.op_begin(), UE = I.op_end(); U != UE; ++U){
    Value* V = U->get();
    unsigned OpIdx = std::distance(I.op_begin(), U);
    TaintMap::iterator OT = TM.find(V);
    if(OT != TM.end() && !OT->second.empty()){
      for(Taint T : OT->second){
        // Create taint source node if there isn't one.
        TaintMap::iterator SN = TG.find(T.first);
        if(SN == TG.end()){
          SN = TG.insert(std::make_pair(T.first, TaintSet())).first;
          Changed = true;
        }
        // Create the edge from taint source TI->first into DN taint sink's
        // operand on index OpIdx.
        Changed = SN->second.insert(Taint(DN->first, OpIdx)) || Changed;
      }
      // If V is a pointer type variable I sinks all it's current taints.
      // Note: sinkTaints should probably only report changes of TG. The
      // clearing of T is done due to the stateful nature of pointer
      // variables.
      if(!isa<Constant>(V) && V->getType()->isPtrOrPtrVectorTy()){
        OT->second.clear();
      }
    }
  }
  return Changed;
}

void WinAPITaintAnalysis::unfoldTaintGraph(TaintMap& TG){
  enum Color{W, G, B};
  MapVector<Value*, Color> C;
  SmallVector<Value*, 20> S;
  // Initialize all node colors to white.
  for(TaintMap::iterator N = TG.begin(), NE = TG.end(); N != NE; ++N){
    C.insert(std::make_pair(N->first, W));
  }
  // Find all back edges using a simple DFS 
  for(TaintMap::iterator N = TG.begin(), NE = TG.end(); N != NE; ++N){
    if(C.find(N->first)->second == W){
      S.push_back(N->first);
    }
    while(!S.empty()){
      Value* U = S.back();
      auto UC = C.find(U);
      if(UC->second == W){
        UC->second = G;
        for(Taint T : TG.find(U)->second){
          auto VC = C.find(T.first);
          if(VC->second == W){
            S.push_back(T.first);
          }else if(VC->second == G){
            errs() << "Found back-edge." << '\n';
          }
        }
      }else if(UC->second == G){
        UC->second = B;
        S.pop_back();
      }
    }
  }
}

void WinAPITaintAnalysis::finalizeTaintGraph(TaintMap& TG){
  // If a node doesn't have all of it's operands tainted, a terminal node
  // needs to be created. The node will be labeled by the type of the operand.
  // If I is a CallInst we should skip handling the called Value.
  MapVector<Value*, SmallSet<int, 10>> RTG;
  for(TaintMap::iterator NI = TG.begin(), NE = TG.end(); NI != NE; ++NI){
    RTG.insert(std::make_pair(NI->first, SmallSet<int, 10>()));
    for(Taint T : NI->second){
      auto RNI = RTG.find(T.first);
      if(RNI == RTG.end()){
        RNI = RTG.insert(std::make_pair(T.first, SmallSet<int, 10>())).first;
      }
      RNI->second.insert(T.second);
    }
  }

  for(auto NI = RTG.begin(), NE = RTG.end(); NI != NE; ++NI){
    if(Instruction* I = dyn_cast<Instruction>(NI->first)){
      for(User::op_iterator U = I->op_begin(), UE = I->op_end(); U != UE; ++U){
        Value* V = U->get();
        unsigned OpIdx = std::distance(I->op_begin(), U);
        if(!NI->second.count(OpIdx)){
          if(CallInst* CI = dyn_cast<CallInst>(I)){
            if(V == CI->getCalledValue()){
              continue;
            }
          }
          TaintMap::iterator SN = TG.find(V);
          if(SN == TG.end()){
            SN = TG.insert(std::make_pair(V, TaintSet())).first;
          }
          SN->second.insert(Taint(I, OpIdx));
        }
      }
    }
  }
}

void WinAPITaintAnalysis::printTaintGraph(TaintMap& TG, Module& M){
  std::stringstream Nodes;
  std::stringstream Edges;
  // Node headers
  Nodes << "# node count" << '\n';
  Nodes << "N " << TG.size() << "\n\n";
  Nodes << "# nodes declarations: V number label in_arity out_arity" << '\n';
  // Edge headers
  Edges << "# edge declarations: ";
  Edges << "E node_from:out_param_from,node_to:in_param_to" << '\n';
  // Iterate through nodes and fill the stringstreams
  for(TaintMap::iterator SI = TG.begin(), SIE = TG.end(); SI != SIE; ++SI){
    // We use the distance of SI from the beginning of the map as an ID.
    unsigned SrcID = std::distance(TG.begin(), SI);
    Nodes << "V " << SrcID << ' ';
    // Fill out the needed name and arities of the node if it's a CallInst,
    // thus a non-terminal node.
    if(CallInst* Src = dyn_cast<CallInst>(SI->first)){
      Nodes << Src->getCalledFunction()->getName().str() << ' ';
      Nodes << Src->getNumArgOperands() << ' ';
      Nodes << (Src->getCalledFunction()->getReturnType()->isVoidTy() ? 0 : 1);
      Nodes << "\n";
    }else{
      // Terminal nodes are treated separately, since they're general Values.
      // If the terminal node is a Function, the return type of the function
      // is used.
      std::string ConstType;
      raw_string_ostream rso(ConstType);
      if(Function* F = dyn_cast<Function>(SI->first)){
        F->getReturnType()->print(rso);
      }else{
        SI->first->getType()->print(rso);
      }
      Nodes << rso.str() << ' ' << "0 1" << '\n';
    }
    // Fill out the edge information
    TaintSet Dst = SI->second;
    for(TaintSet::iterator DI = Dst.begin(), DIE = Dst.end(); DI != DIE; ++DI){
      unsigned DstID = std::distance(TG.begin(), TG.find(DI->first));
      Edges <<  "E " << SrcID << ":0," << DstID << ':' << DI->second << '\n';
    }
  }
  // Final SDG file printing.
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