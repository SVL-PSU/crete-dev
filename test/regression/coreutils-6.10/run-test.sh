#!/bin/sh

CRETE_BINARY_DIR=`readlink -e ../../../bin`

if [[ -z $CRETE_BINARY_DIR ]]; then
	echo "Error: Binary directory not found."
	echo "Ensure your are running this from the build directory (not the source directory)."
	exit 1
fi

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETE_BINARY_DIR

DISPATCH=$CRETE_BINARY_DIR/crete-dispatch
VM_NODE=$CRETE_BINARY_DIR/crete-vm-node
SVM_NODE=$CRETE_BINARY_DIR/crete-svm-node
COVERAGE=$CRETE_BINARY_DIR/crete-coverage

if [[ ! -f $DISPATCH ]]; then
	echo "Error: crete-dispatch not found"
	exit 1
fi
if [[ ! -f $VM_NODE ]]; then
	echo "Error: crete-vm-node not found"
	exit 1
fi
if [[ ! -f $SVM_NODE ]]; then
	echo "Error: crete-svm-node not found"
	exit 1
fi
if [[ ! -f $COVERAGE ]]; then
	echo "Error: crete-coverage not found"
	exit 1
fi

$DISPATCH -c crete.dispatch.xml &
DISPATCH_PID=$!
sleep 3
$VM_NODE -c crete.vm-node.xml &
VM_NODE_PID=$!
sleep 3
$SVM_NODE -c crete.svm-node.xml &
SVM_NODE_PID=$!

wait $DISPATCH_PID

rm -f coverages.txt

for dir in dispatch/last/*
do
	prog=${dir%.*}
	prog=${prog##*.}

	echo "Generating coverage report for: $prog " $dir 
	
	echo $prog >> coverages.txt
	$COVERAGE -t "$dir/test-case" -c cov.config.xml -e "coreutils-6.10/src/$prog" > /dev/null
	cd coreutils-6.10/src && gcov -b "coreutils-6.10/src/$prog" >> ../../coverages.txt
	cd ../..
done

diff coverages.txt expected-coverages.txt > coverages.diff
ret=$?
if [ $ret -eq 0 ]
then
	echo "Success: Coverage report is consistent with expected report."
elif [ $ret -eq 1 ]
then
	echo "Failure: Coverage report is INCONSISTENT with expected report. See coverage.diff"
else
	echo "Failure: An error occurred while attempting to diff coverage report with expected coverage report."
fi
