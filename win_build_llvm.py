import os
import sys
import shutil
import subprocess

def main():
    ConfGen = "Visual Studio 14 2015 Win64"
    ConfFlags = "-DLLVM_TARGETS_TO_BUILD:STRING=X86"
    ConfSrcDir = "../llvm-3.7.0"

    BuildDir = "./tools/"
    BuildType = "Release"
    BuildJobs = "/maxcpucount:2"

    if len(sys.argv) == 2 and sys.argv[1] == "clean":
        shutil.rmtree(BuildDir)

    if not os.path.exists(BuildDir):
        os.mkdir(BuildDir)
    
    os.chdir(BuildDir)

    subprocess.call(["cmake", "-G", ConfGen, ConfFlags, ConfSrcDir])
    subprocess.call(["cmake", "--build", '.', "--config", BuildType, "--", BuildJobs])
    
if __name__ == '__main__':
    main()