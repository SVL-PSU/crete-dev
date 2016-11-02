#!/bin/sh

$CRETEPATH/klee/Release+Asserts/bin/klee -search=dfs -randomize-fork=false -concolic-mode=true run.bc &> concolic.log
$CRETEPATH/klee/Release+Asserts/bin/klee -search=dfs -randomize-fork=false run.bc &> klee-run.log
