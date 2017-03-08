# ! /bin/bash -x
INPUT_DIR=$(readlink -m $1)
printf "$INPUT_DIR\n"

cd $INPUT_DIR
/home/chenbo/crete/crete-dev/misc/scripts/replay/batch_replay.sh $INPUT_DIR /home/chenbo/crete/crete-dev/misc/scripts/replay/br_coreutils.sh &> dispatch_batch_replay.log
