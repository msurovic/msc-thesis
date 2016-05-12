import os
import sys
import subprocess
import random
import argparse

def main():
    parser = argparse.ArgumentParser(description='Generate mutations using ollvm.')
    parser.add_argument('-sub', action='store_true',
        help='Enable instruction substitution.')
    parser.add_argument('-fla', action='store_true',
        help='Enable control flow flattening.')
    parser.add_argument('-bcf', action='store_true',
        help='Enable bogus control flow.')
    parser.add_argument('-bcf-loop', type=int, default=1,
        help='How many loops will bcf perform on a function.')
    parser.add_argument('-spli', action='store_false',
        help='Enable basic block splitting.')
    parser.add_argument('-r', action='store_true',
        help='Apply all specified obfuscations randomly.')
    parser.add_argument('-t', type=str, default=os.path.join(os.getcwd(), 'toolchain'),
        help='Path to the ollvm toolchain.')
    parser.add_argument('-m', type=int, default=10, 
        help='Number of mutations to generate.')
    parser.add_argument('-i', type=str, default=os.path.join(os.getcwd(), 'bitcodes'),
        help='Specify input folder.')
    parser.add_argument('-o', type=str, default=os.path.join(os.getcwd(), 'mutations'),
        help='Specify output folder.')
    
    args = parser.parse_args()

    obf = []

    if args.sub:
        obf.append(['-mllvm', '-sub'])
    if args.fla:
        obf.append(['-mllvm', '-fla'])
    if args.bcf:
        obf.append(['-mllvm', '-bcf', '-mllvm', '-boguscf-loop=' + str(args.bcf_loop)])
    if args.spli:
        obf.append(['-mllvm', '-spli'])
    
    clang = os.path.join(args.t, 'clang')

    if not os.path.exists(clang) and not os.path.exists(clang + '.exe'):
        print 'Error: clang was not found in ' + args.t
        sys.exit(1)

    if not os.path.exists(args.i):
        print 'Error: input folder' + args.i + 'does not exist'
        sys.exit(1)

    if not os.path.exists(args.o):
        os.mkdir(args.o)

    for file in filter(lambda x:x.endswith('.bc'), os.listdir(args.i)):
        print 'Generating for file ' + file
        for i in range(args.m):
            flags = [['-c', '-emit-llvm']]
            seed  = os.urandom(16).encode('hex')
            
            if args.r:
                flags += random.sample(obf, random.randint(0, len(obf)-1))
            else:
                flags += obf

            flags = [item for sublist in flags for item in sublist]
            
            if len(obf) > 0:
                flags += ['-mllvm', '-aesSeed=' + '"' + seed + '"']

            infile  = os.path.join(args.i, file)
            outfile = file[:-3] + '-' + seed[:8] + '.bc'
            outpath = os.path.join(args.o, outfile)

            #print [clang, infile] + flags + ['-o', outpath]
            subprocess.call([clang, infile] + flags + ['-o', outpath])

if __name__ == '__main__':
    main()
