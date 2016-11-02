#!/bin/sh

# To generate branch coverage:
# editing your .lcovrc file (copied from /etc/lcovrc) to change lcov_branch_coverage setting to 1

HOST_CRETE_BIN_DIR="/home/chenbo/crete/chenbo-qemullvm/trunk/build/bin/"
CRETE_BIN_DIR=$HOST_CRETE_BIN_DIR

PARSEGCOVCMD=$CRETE_BIN_DIR/../../misc/scripts/coverage/parse_gcov_coreutils.py

HOST_PROG_DIR="/home/chenbo/icse2016/eval/coreutils-6.10/crete-coverage/src"
PROG_DIR=$HOST_PROG_DIR

# GUEST_CRETE_BIN_DIR="/home/test/guest-build/bin"
# CRETE_BIN_DIR=$GUEST_CRETE_BIN_DIR

# PARSEGCOVCMD=$CRETE_BIN_DIR/../../guest/scripts/coverage/parse_gcov_coreutils.py

# GUEST_PROG_DIR="/home/test/tests/coreutils-6.10/coverage/src/"
# PROG_DIR=$GUEST_PROG_DIR

PROGRAMS="base64
basename
cat
cksum
comm
cut
date
df
dircolors
dirname
echo
env
expand
expr
factor
fmt
fold
head
hostid
id
join
logname
ls
nl
od
paste
pathchk
pinky
printenv
printf
pwd
readlink
seq
shuf
sleep
sort
stat
sum
sync
tac
tr
tsort
uname
unexpand
uptime
users
wc
whoami
who"

PROGRAMS="base64
basename"

REPLAYCMD=$CRETE_BIN_DIR/crete-replay
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETE_BIN_DIR

INPUT_DIR=$1

main()
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
    foldername="replay$(date +[%Y-%m-%d_%T])"
    mkdir $foldername
    cd $foldername

    printf "1. Cleanup old coverage info...\n"
    lcov --directory $PROG_DIR --zerocounters

    printf "2. execute all test cases...\n"
    for prog in $PROGRAMS
    do
        printf "\texecuting $prog...\n"
        $REPLAYCMD -e $PROG_DIR/$prog -c $TC_DIR/auto_$prog.xml/guest-data/crete-guest-config.serialized -t $TC_DIR/auto_$prog.xml/test-case
    done

    printf "3. Parsing crete.replay.log ...\n"
    printf "Assertion failed: "
    grep -c -w "Assertion.*failed." crete.replay.log
    printf "Exception caught: "
    grep -c -w "\[crete-replay-preload\] Exception"  crete.replay.log
    printf "Signal caught: "
    grep -c -w "\[Signal Caught\]"  crete.replay.log

    printf "4. generating coverage report... \n"
    lcov --base-directory $PROG_DIR --directory $PROG_DIR --capture --output-file lcov.info --rc lcov_branch_coverage=1 >> lcov.log
    genhtml lcov.info -o html --function-coverage --rc lcov_branch_coverage=1 >> lcov.log

    $PARSEGCOVCMD $PROG_DIR &> result_gcov.org

    printf "5. Finished: $foldername/result_gcov.org and $foldername/html/index.html\n"
    tail -4 lcov.log
}

main
