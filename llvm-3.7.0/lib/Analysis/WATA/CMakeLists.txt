add_llvm_library(LLVMWATA
  TaintAnalysis.cpp
  )

add_dependencies(LLVMWATA intrinsics_gen)
