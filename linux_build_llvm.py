import os
import sys
import shutil
import subprocess

def main():
    ConfGen = "Unix Makefiles"
    ConfTarget = "-DLLVM_TARGETS_TO_BUILD:STRING=X86"
    ConfBuild  = "-DCMAKE_BUILD_TYPE=Release"
    ConfSrcDir = "../llvm-3.7.0"

    BuildDir = "./toolchain/"

    if len(sys.argv) == 2 and sys.argv[1] == "clean":
        shutil.rmtree(BuildDir)

    if not os.path.exists(BuildDir):
        os.mkdir(BuildDir)
    
    os.chdir(BuildDir)

    subprocess.call(["cmake", "-G", ConfGen, ConfTarget, ConfBuild,ConfSrcDir])
    subprocess.call(["cmake", "--build", '.'])
    
if __name__ == '__main__':
    main()
