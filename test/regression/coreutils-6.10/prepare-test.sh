#!/bin/sh

CRETE_SOURCE_DIR=$1
CRETE_BINARY_DIR=$2

cp $CRETE_SOURCE_DIR/run-test.sh $CRETE_BINARY_DIR/
cp $CRETE_SOURCE_DIR/crete.dispatch.xml $CRETE_BINARY_DIR/
cp $CRETE_SOURCE_DIR/crete.vm-node.xml $CRETE_BINARY_DIR/
cp $CRETE_SOURCE_DIR/crete.svm-node.xml $CRETE_BINARY_DIR/
cp $CRETE_SOURCE_DIR/cov.config.xml $CRETE_BINARY_DIR/
cp $CRETE_SOURCE_DIR/expected-coverages.txt $CRETE_BINARY_DIR/

cd $CRETE_BINARY_DIR && wget svl13.cs.pdx.edu/crete.0929.img
mv crete.0929.img crete.img
