# To generate branch coverage:
# editing your .lcovrc file (copied from /etc/lcovrc) to change lcov_branch_coverage setting to 1

# HOST_CRETE_BIN_DIR="/home/chenbo/crete/crete-dev/build/bin/"
# HOST_PROG_DIR="/home/chenbo/crete/ffmpeg-test/coverage/ffmpeg-3.1.2"

# CRETE_BIN_DIR=$HOST_CRETE_BIN_DIR
# PARSEGCOVCMD=$CRETE_BIN_DIR/../../misc/scripts/coverage/parse_gcov_coreutils.py
# PROG_DIR=$HOST_PROG_DIR

GUEST_CRETE_BIN_DIR="/home/test/guest-build/bin"
GUEST_PROG_DIR="/home/test/tests/ffmpeg-3.1.2/coverage"

CRETE_BIN_DIR=$GUEST_CRETE_BIN_DIR
PARSEGCOVCMD=$CRETE_BIN_DIR/../../guest/scripts/replay/parse_gcov_coreutils.py
PROG_DIR=$GUEST_PROG_DIR

PROGRAMS="ffmpeg
ffprobe"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETE_BIN_DIR
