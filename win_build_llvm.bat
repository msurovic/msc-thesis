mkdir tools
cd tools
cmake -G "Visual Studio 14 2015 Win64" -DLLVM_TARGETS_TO_BUILD:STRING=X86 ..\llvm-3.7.0
cmake --build . --config Release -- /maxcpucount:2
cd ..