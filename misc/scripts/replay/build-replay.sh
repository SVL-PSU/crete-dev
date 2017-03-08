#!/bin/bash -x

tar_coreutils="/home/chenbo/crete/crete-dev/front-end/guest/evals/coreutils-6.10.tar.gz"
sandbox_dir="/home/chenbo/crete/crete-dev/front-end/guest/sandbox/klee-sandbox.tgz"
crete_replay_dir="/home/chenbo/crete/replay-evals"

if [ ! -f $tar_coreutils ]; then
    printf "$tar_coreutils does not exist\n"
    exit
fi

if [ ! -f $sandbox_dir ]; then
    printf "$sandbox_dir does not exist\n"
    exit
fi

rm -rf $crete_replay_dir
mkdir -p $crete_replay_dir

cd $crete_replay_dir
tar xf $sandbox_dir

tar xf $tar_coreutils
mkdir coreutils-6.10/coverage
cd coreutils-6.10/coverage
../configure --disable-nls CFLAGS="-g -fprofile-arcs -ftest-coverage"
make -j7
make -C src arch hostname
