#!/bin/bash

REF_1=$1
REF_2=$2

TEST_CASE_DIR="/test-case/"
main()
{
    # check Macros which should be defined in $INCLUDE_FILE
    if [ ! -d  $REF_1 ]; then
        printf "\$REF_1 is invalid ...\n"
        exit
    fi

    if [ ! -d  $REF_2 ]; then
        printf "\$REF_2 is invalid ...\n"
        exit
    fi

    for d in $REF_1/*/ ; do
        sub=$(echo $d | cut -d'/' -f 3)

        TC_DIR_1=$d$TEST_CASE_DIR
        TC_DIR_2=$REF_2$sub$TEST_CASE_DIR

        if [ ! -d $TC_DIR_1 ]; then
            continue
        elif [ ! -d $TC_DIR_2 ]; then
            continue
        fi

        diff -qr $TC_DIR_1 $TC_DIR_2 | grep diff | wc -l  | \
            { read diff_count; test $diff_count -ne 0 && printf "$sub\t\t$diff_count\n"; }
    done
}

main
