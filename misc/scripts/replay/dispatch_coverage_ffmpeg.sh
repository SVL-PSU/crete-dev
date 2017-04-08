# ! /bin/bash -x
CRETEPATH='/home/chenbo/crete/build/bin'
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETEPATH:/usr/local/lib:$CRETEPATH/boost

INPUT_DIR=$(readlink -m $1)
printf "$INPUT_DIR\n"

$CRETEPATH/crete-tc-compare -b $INPUT_DIR


cd $INPUT_DIR
/home/chenbo/crete/crete-dev/misc/scripts/replay/batch_replay.sh $INPUT_DIR /home/chenbo/crete/crete-dev/misc/scripts/replay/br_ffmpeg.sh &> dispatch_batch_replay.log
