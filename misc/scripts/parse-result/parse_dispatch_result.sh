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
    grep -c '\[2016\-' $INPUT_DIR/log/node_error.log
    printf "SVM error: "
    grep -c -w 'Node: SVM' $INPUT_DIR/log/node_error.log

    printf "cross-check failed: "
    grep -c "QemuRuntimeInfo::cross_check_cpuState.*failed." < $INPUT_DIR/log/node_error.log

    printf "Taint analysis check failed: "
    grep -c "Assertion.*crete_dbg_ta_fail.*failed." < $INPUT_DIR/log/node_error.log

    printf "missing helper functions: "
    grep -c "Executor.cpp\:2939.*Assertion.*failed." < $INPUT_DIR/log/node_error.log

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
    printf "Overshift error: "
    grep -c "^KLEE: ERROR: (location information missing) overshift error$" < $INPUT_DIR/log/node_error.log
    printf "STP timed out: "
    grep -c "^error: STP timed out" < $INPUT_DIR/log/node_error.log
    printf "Over memory cap: "
    grep -c "^KLEE\: WARNING\: killing 1 states (over memory cap)" < $INPUT_DIR/log/node_error.log
    printf "HaltTimer: "
    grep -c "KLEE: HaltTimer invoked" < $INPUT_DIR/log/node_error.log
    printf "Invalid test case: "
    grep -c "Invalid test case" < $INPUT_DIR/log/node_error.log

    printf "\nVM error: "
    grep -c -w 'Node:\sVM' $INPUT_DIR/log/node_error.log
    printf "getHostAddress() error: "
    grep -c -w 'getHostAddress\(\)' $INPUT_DIR/log/node_error.log
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
        grep -c "2016.*$folder" $INPUT_DIR/log/node_error.log | \
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
    error_detail
    last_tc_time
    finish_log
}

main
