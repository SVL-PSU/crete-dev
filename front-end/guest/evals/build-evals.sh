#!/bin/bash
input_dir=$1
work_dir="/home/test/tests"

rm -rf $work_dir
mkdir -p $work_dir

cd $work_dir
tar xf $1/coreutils-6.10.tar.gz
mkdir coreutils-6.10/build
cd coreutils-6.10/build
../configure --disable-nls  --prefix=$work_dir/build
make
make -C src arch hostname
make install
cp src/hostname $work_dir/build/bin
cp src/ginstall $work_dir/build/bin
cp src/setuidgid $work_dir/build/bin

# cd $work_dir
# tar xf $1/diffutils-3.3.tar.xz
# mkdir diffutils-3.3/build
# cd diffutils-3.3/build
# ../configure --disable-nls  --prefix=$work_dir/build
# make
# make install

# cd $work_dir
# tar xf $1/grep-2.18.tar.xz
# mkdir grep-2.18/build
# cd grep-2.18/build
# ../configure --disable-nls  --prefix=$work_dir/build
# make
# make install


## for coverage setup
# ../configure --disable-nls  --prefix=$work_dir/exec CFLAGS="-g -fprofile-arcs -ftest-coverage"
