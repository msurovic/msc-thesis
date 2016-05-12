import os
import sys
import subprocess
import shutil
import argparse

def main():
    parser = argparse.ArgumentParser(description='Generate signatures using opt and WATA.')
    parser.add_argument('-t', type=str, default=os.path.join(os.getcwd(), 'toolchain'),
        help='Path to the ollvm toolchain.')
    parser.add_argument('-i', type=str, default=os.path.join(os.getcwd(), 'mutations'),
        help='Specify input folder.')
    parser.add_argument('-o', type=str, default=os.path.join(os.getcwd(), 'signatures'),
        help='Specify output folder.')

    args = parser.parse_args()
    opt  = os.path.join(args.t, 'opt')

    if not os.path.exists(opt) and not os.path.exists(opt + '.exe'):
        print 'Error: opt was not found in ' + args.t
        sys.exit(1)

    if not os.path.exists(args.i):
        print 'Error: input folder' + args.i + 'does not exist'
        sys.exit(1)

    if not os.path.exists(args.o):
        os.mkdir(args.o)

    for file in filter(lambda x:x.endswith('.bc'), os.listdir(args.i)):
        print 'Analyzing file ' + file

        infile  = os.path.join(args.i, file)
        inlined = os.path.join(args.i, file[:-3] + '-inlined.bc')
        sdgfile = os.path.join(args.i, inlined + '.sdg')
        outfile = os.path.join(args.o, file.replace('.bc', '.sdg'))

        if not os.path.exists(inlined):
            subprocess.call([opt, '-prep-always-inline', '-always-inline', infile, '-o', inlined])
        
        subprocess.call([opt, '-winapi-ta', inlined], stdout=open(os.devnull, "w"))
        shutil.move(sdgfile, outfile)

if __name__ == '__main__':
    main()
