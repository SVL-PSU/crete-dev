INPUT_DIR=$1
declare -a folder_list

error_summary()
{
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"
    printf "Error Summary\n"
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"

    if [ ! -e  $INPUT_DIR/log/node_error.log ]; then
        printf "$INPUT_DIR/log/node_error.log does not exists\n"
        return
    fi

    printf "Totoal error: "
    grep -c '\[2017\-' $INPUT_DIR/log/node_error.log

    printf "\nVM error: "
    grep -c -w 'Node:\sVM' $INPUT_DIR/log/node_error.log
    printf "getHostAddress() error: "
    grep -c -w 'getHostAddress\(\)' $INPUT_DIR/log/node_error.log
    printf "trace-tag-error: "
    grep -c "crete-qemu-2.3-system-i386:.*void RuntimeEnv::add_trace_tag(const TranslationBlock\*, uint64_t): Assertion .* failed."  < $INPUT_DIR/log/node_error.log

    printf "\nSVM error: "
    grep -c -w 'Node: SVM' $INPUT_DIR/log/node_error.log

    printf "\nKLEE Warning:"
    grep -c "KLEE: WARNING: " < $INPUT_DIR/log/node_error.log

    printf "KLEE: WARNING: unable to compute initial values: "
    grep -c "KLEE: WARNING: unable to compute initial values" < $INPUT_DIR/log/node_error.log
    printf "KLEE: WARNING: max-intruction-time execeeded: "
    grep -c "KLEE: WARNING: max-instruction-time exceeded" < $INPUT_DIR/log/node_error.log
    printf "KLEE: WARNING: STP timed out: "
    grep -c "KLEE: WARNING: STP timed out" < $INPUT_DIR/log/node_error.log

    printf "\nOther KLEE prints:\n"
    printf 'vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n'
    grep "KLEE" $INPUT_DIR/log/node_error.log \
        | grep -v "KLEE: output directory is " | grep -v "KLEE: done" \
        | grep -v "KLEE: WARNING: unable to compute initial values" \
        | grep -v "KLEE: WARNING: max-instruction-time exceeded"  \
        | grep -v "KLEE: WARNING: STP timed out"
    printf '^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n\n'

    printf "cross-check failed: "
    grep -c "QemuRuntimeInfo::cross_check_cpuState.*failed." < $INPUT_DIR/log/node_error.log

    printf "Taint analysis check failed: "
    grep -c "Assertion.*crete_dbg_ta_fail.*failed." < $INPUT_DIR/log/node_error.log

    printf "missing helper functions: "
    grep -c "^\[CRETE ERROR\] Calling external function.* missing in helper.bc from qemu." < $INPUT_DIR/log/node_error.log

    printf "\nLoad MO missing: "
    grep -c "Load\sMO\smissing\!" < $INPUT_DIR/log/node_error.log
    printf "overlapped with symbolics: "
    grep -c "The\sgiven\sMO\sis\soverlapped\swith\ssymbolics\sMO" < $INPUT_DIR/log/node_error.log
    printf "allocate overlapping object: "
    grep -c "Trying\sto\sallocate\san\soverlapping\sobject" < $INPUT_DIR/log/node_error.log

    printf "\ndebug_xmm: "
    grep -c "debug_xmm" < $INPUT_DIR/log/node_error.log
    printf "debug_fpregs: "
    grep -c "debug_fpregs" < $INPUT_DIR/log/node_error.log
    printf "debug_regs: "
    grep -c "debug_regs" < $INPUT_DIR/log/node_error.log

    printf "\nError Reported by KLEE:\n"
    printf "HaltTimer: "
    grep -c "KLEE: HaltTimer invoked" < $INPUT_DIR/log/node_error.log
    printf "max-instruction-time exceeded: "
    grep -c "max-instruction-time exceeded" < $INPUT_DIR/log/node_error.log
    printf "Over memory cap: "
    grep -c "^KLEE\: WARNING\: killing 1 states (over memory cap)" < $INPUT_DIR/log/node_error.log
    printf "Overshift error: "
    grep -c "^KLEE: ERROR: (location information missing) overshift error$" < $INPUT_DIR/log/node_error.log
    printf "Invalid test case: "
    grep -c "Invalid test case" < $INPUT_DIR/log/node_error.log
    printf "Trace-tag: forking not from captured bitcode: "
    grep -c "klee: Executor.cpp.*Executor::StatePair klee::Executor::crete_concolic_fork(klee::ExecutionState &, ref<klee::Expr>).*\[CRETE FIXME\] klee forks not from captured bitcode.* failed." < $INPUT_DIR/log/node_error.log
}

error_detail()
{
    printf "\n"
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"
    printf "Error occurence\n"
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"


    if [ ! -e  $INPUT_DIR/log/node_error.log ]; then
        printf "$INPUT_DIR/log/node_error.log does not exists\n"
        return
    fi

    for folder in "${folder_list[@]}"
    do
        grep -c "2017.*$folder" $INPUT_DIR/log/node_error.log | \
            { read error_count; \
            trace_num=$(tail -1 $INPUT_DIR/$folder/log/finish.log | grep -P '\d+' -o | awk 'NR==5'); \
            failure_percent=$((100*$error_count/$trace_num)); \
            test $error_count -ne 0 && printf "%-20s\t$error_count/$trace_num ($failure_percent%%)\n" $folder ;}
    done
}

last_tc_time()
{
    printf "\n"
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"
    printf "last tc time\n"
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"
    for folder in "${folder_list[@]}"
    do
	printf "%-20s\t" $folder
        (stat --printf '%Y' $INPUT_DIR/$folder/log/; printf ' - '; stat --printf '%Y\n' ./$folder/test-case/) | bc -l \
            | { read last_tc_time; printf "%-10s\n" $last_tc_time; }
    done
}

finish_log()
{
    printf "\n"
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"
    printf "finish log\n"
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"
    for folder in "${folder_list[@]}"
    do
	printf "$folder\n"
        cat $INPUT_DIR/$folder/log/finish.log
    done
}

duplicated_tc()
{
    printf "\n"
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"
    printf "Duplicated Test case\n"
    printf "++++++++++++++++++++++++++++++++++++++++++++++\n"


    for folder in "${folder_list[@]}"
    do
        num_total_tc=$(tail -1 $INPUT_DIR/$folder/log/finish.log | grep -P '\d+' -o | awk 'NR==3')
        num_tc_pool=$(tail -1 $INPUT_DIR/$folder/log/finish.log | grep -P '\d+' -o | awk 'NR==2')
        num_tc_vm=$(tail -1 $INPUT_DIR/$folder/log/finish.log | grep -P '\d+' -o | awk 'NR==6')
        num_dup_tc=$(awk '{print($6)}' $INPUT_DIR/$folder/log/test_case_tree.log)
        num_issued_tc=$((((num_total_tc-num_tc_pool)-num_tc_vm)-num_dup_tc))

        num_trace_pool=$(tail -1 $INPUT_DIR/$folder/log/finish.log | grep -P '\d+' -o | awk 'NR==5')
        num_trace_vm=$(tail -1 $INPUT_DIR/$folder/log/finish.log | grep -P '\d+' -o | awk 'NR==7')
        num_trace_total=$((num_trace_pool+num_trace_vm))

        num_vm_error=$(grep -A 10 "2017.*$folder" $INPUT_DIR/log/node_error.log | grep -c -w 'Node:\sVM')

        # 1. check trace/tc number consistency
        # num_issued_tc == num_trace_total + num_vm_error
        should_be_zero_or_one=$(((num_issued_tc-num_trace_total)-num_vm_error))

        if [[ "$should_be_zero_or_one" -ne "0" ]] && [[ "$should_be_zero_or_one" -ne "1" ]]; then
            printf "%s: inconsistent tc/trace number ($should_be_zero_or_one)\n" $folder
        fi

        # 2. duplicated tc number
        if [[ "num_dup_tc" -ne "0" ]]; then
            failure_percent=$((100*$num_dup_tc/$num_issued_tc)); \
            printf "%s duplicated tc: $num_dup_tc/$num_issued_tc ($failure_percent%%)\n" $folder
        fi

    done
}

init_folder_list()
{
    if [[ -z $INPUT_DIR ]]; then
        printf "INPUT_DIR is not given...\n"
        exit
    fi

    INPUT_DIR=$(readlink -m $INPUT_DIR)

    printf "Input direcotry: $INPUT_DIR\n"

    if [ ! -d  $INPUT_DIR ]; then
        printf "$INPUT_DIR does not exists\n"
        exit
    fi

    SUB_FOLDERS=$INPUT_DIR/*.xml
    for f in $SUB_FOLDERS
    do
        if [ ! -d $f ]; then
            continue
        fi

        # only get the name of subfolders
        input=$f
        while [[ $input == *'/'* ]]; do
            input=${input#*/}
        done

        folder_list+=($input)
    done

    # for folder in "${folder_list[@]}"
    # do
    #     printf "$folder\n"
    # done
}


main()
{
    init_folder_list
    error_summary
    duplicated_tc
    error_detail
    last_tc_time
    finish_log
}

main
