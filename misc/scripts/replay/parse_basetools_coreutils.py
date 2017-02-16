#!/usr/bin/python

import subprocess
import re
import os
import sys

# sample
## gcov 4.8
# File '../../src/cat.c'
# Lines executed:68.97% of 232
# Branches executed:67.79% of 149
# Taken at least once:51.01% of 149
# Calls executed:57.14% of 70
# Creating 'cat.c.gcov'

programs = ["bootsectimage", "efirom", "GenFfs", "GenFw", "GenSec", "GnuGenBootSector",
        "Split", "VfrCompile", "EfiLdrImage", "GenCrc32", "GenFv", "GenPage", "GenVtf",
        "LzmaCompress", "TianoCompress", "VolInfo"]

def parse_cov(cov, prog):
    blocks = re.split('File.*\n', cov)
    for block in blocks:
        got = re.search('Creating \''+prog+'.c.gcov\'', block, re.MULTILINE)
        if got is not None:
            # print block
            ret1 = re.search('Lines executed:(\d+.\d+%) of (\d+)', block)
            ret2 = re.search('Branches executed:(\d+.\d+%) of (\d+)', block)
            ret3 = re.search('Taken at least once:(\d+.\d+%) of (\d+)', block)
            ret4 = re.search('Calls executed:(\d+.\d+%) of (\d+)', block)
            print  '|', prog ,'|', ret1.group(1), '|', ret1.group(2), '|', ret2.group(1), '|', ret2.group(2), '|', ret3.group(1), '|', ret3.group(2), '|', ret4.group(1), '|', ret4.group(2), '|'
            return [ret1.groups(), ret2.groups(), ret3.groups(), ret4.groups()]


def get_cov():
    if len(sys.argv) < 3:
        print "Please input the directory of baseTool executables...\n"
        sys.exit(0)

    obj_gcov_root_dir = sys.argv[1]
    if not (os.path.isdir(obj_gcov_dir)):
        print "obj_gcov_dir is invalid..\n"
        sys.exit(0)

    print '|------+-------+------+---------+--------+-----+----+-------+------|'
    print '| Prog | Line% | Line | Branch% | Branch | B1% | B1 | Call% | Call |'
    print '|------+-------+------+---------+--------+-----+----+-------+------|'

    for prog in programs:
        working_gcov_dir = obj_gcov_root_dir
        os.chdir(working_gcov_dir)
        cov = subprocess.check_output(['gcov', '-b', prog])
        parse_cov(cov, prog)
    print '|------+-------+------+---------+--------+-----+----+-------+------|'

def main():
    get_cov()

if __name__ == "__main__":
    main()
