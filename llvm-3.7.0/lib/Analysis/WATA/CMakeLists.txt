add_llvm_library(WATA
  TaintAnalysis.cpp
  )

add_dependencies(WATA intrinsics_gen)
