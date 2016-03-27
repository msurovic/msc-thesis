add_llvm_library(LLVMwata
  TaintAnalysis.cpp
  PrepAlwaysInline.cpp
  )

add_dependencies(LLVMwata intrinsics_gen)