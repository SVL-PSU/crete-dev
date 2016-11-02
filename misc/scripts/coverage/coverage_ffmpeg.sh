#!/bin/sh
CRETE_BIN_DIR="/home/chenbo/crete/chenbo-qemullvm/trunk/build/bin/"

COVCMD=$CRETE_BIN_DIR/crete-coverage
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETE_BIN_DIR

PROG_DIR="/home/chenbo/crete/ffmpeg-test/coverage/ffmpeg-3.1.2"
PROGRAMS="ffmpeg
ffprobe"

INPUT_DIR=$1

function main
{
    # check input dir
    if [ -z  $CRETE_BIN_DIR ]; then
        printf "\$CRETE_BIN_DIR is invalid ...\n"
        exit
    fi

    if [ -z  $INPUT_DIR ]; then
        printf "Input directory is invalid ...\n"
        exit
    fi

    TC_DIR=$(readlink -m $INPUT_DIR)

    printf "Input direcotry: $TC_DIR\n"

    if [ ! -d  $TC_DIR ]; then
        printf "$TC_DIR does not exists\n"
        exit
    fi

    # create new folder to put result
    foldername="coverage$(date +[%Y-%m-%d_%T])"
    mkdir $foldername
    cd $foldername

    mv $PROG_DIR/crete-coverage-fate/fate-suite/ $PROG_DIR/crete-coverage-fate/tests/ .

    printf "1. Cleanup old coverage info...\n"
    lcov --directory $PROG_DIR --zerocounters

    printf "2. execute all test cases in folder $TC_DIR ...\n"
    SUB_FOLDERS=$TC_DIR/*.xml
    for f in $SUB_FOLDERS
    do
        target_prog="not-found"
        # 2.1 scan for target executable (assumption: the subfolder contains the target-executable's name)
        for prog in $PROGRAMS
        do
            if [[ $f == *$prog* ]]
            then
                target_prog=$prog
            fi
        done

        if [ $target_prog == "not-found" ]; then
            printf "[Warning] No executable from list \"\$PROGRAMS\" matches subfolder $f! Skip it ...\n"
            continue
        fi

        # execute all the test cases from the current subfolder with target_prog
        printf "executing $target_prog with test-case from $f...\n"
        $COVCMD -g -e $PROG_DIR/$target_prog -c $f/guest/crete-guest-config.xml -t $f/test-case >> crete-coverage-progress.log
    done

    printf "3. generating coverage report... \n"
    $CRETE_BIN_DIR/../../misc/scripts/coverage/parse_gcov_coreutils.py $PROG_DIR &> result_gcov.org

    lcov --base-directory $PROG_DIR --directory $PROG_DIR --capture --output-file lcov.info --rc lcov_branch_coverage=1 >> lcov.log
    genhtml lcov.info -o html --function-coverage --rc lcov_branch_coverage=1 >> lcov.log

    printf "4. Finished: $foldername/result_gcov.org and $foldername/html/index.html\n"
    tail -4 lcov.log

    mv ./fate-suite/ ./tests/ $PROG_DIR/crete-coverage-fate
}

main
