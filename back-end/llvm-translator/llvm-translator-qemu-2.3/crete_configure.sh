#!/bin/bash

CRETE_SOURCE_DIR=$1
CRETE_BINARY_DIR=$2

 ./configure \
 	--enable-tcg-interpreter \
 	--enable-llvm \
	--with-llvm=$CRETE_BINARY_DIR/lib/llvm/llvm-3.2-prefix/src/llvm-3.2/Release+Asserts/ \
 	--target-list=i386-softmmu,x86_64-softmmu \
	--extra-ldflags="-L$CRETE_BINARY_DIR/bin" \
 	--extra-cflags="-mno-sse3 -g\
 		-O0 \
 		-I../../lib/include \
 		-DTCG_LLVM_OFFLINE" \
 	--extra-cxxflags="-mno-sse3 \
 		-O2 \
 		-DTCG_LLVM_OFFLINE"

