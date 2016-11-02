#!/bin/bash

CRETE_SOURCE_DIR=$1
CRETE_BINARY_DIR=$2

DEBUG= ./configure \
 	--enable-tcg-interpreter \
 	--target-list=i386-softmmu,x86_64-softmmu \
	--extra-cflags="-I$CRETE_SOURCE_DIR/lib/include" \
	--extra-cxxflags="-I$CRETE_SOURCE_DIR/lib/include" \
	--extra-ldflags="-L$CRETE_BINARY_DIR/bin"
