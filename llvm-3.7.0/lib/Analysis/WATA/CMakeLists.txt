add_llvm_library(LLVMwata
  TaintAnalysis.cpp
  )

add_dependencies(LLVMwata intrinsics_gen)