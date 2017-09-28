# To generate branch coverage:
# editing your .lcovrc file (copied from /etc/lcovrc) to change lcov_branch_coverage setting to 1
HOST_CRETE_BIN_DIR="/home/chenbo/crete/build/bin/"
HOST_PROG_DIR="/home/chenbo/crete/replay-evals/FFmpeg/"

CRETE_BIN_DIR=$HOST_CRETE_BIN_DIR
PARSEGCOVCMD=$CRETE_BIN_DIR/../../crete-dev/misc/scripts/replay/parse_gcov_coreutils.py
PROG_DIR=$HOST_PROG_DIR
LCOV_DIR=$HOST_PROG_DIR
UNIFIED_ENV="/home/chenbo/crete/crete-dev/front-end/guest/sandbox/env/klee-test.env"

# CHECK_EXPLOITABLE_SCRIPT="/home/chenbo/crete/crete-dev/misc/util/tc-replay/check-exploitable/exploitable/exploitable.py"
# LAUNCH_DIR="/home/chenbo/crete/replay-evals/sandbox"
# SANDBOX="/home/chenbo/crete/replay-evals/sandbox"

# GUEST_CRETE_BIN_DIR="/home/test/guest-build/bin"
# GUEST_PROG_DIR="/home/test/tests/ffmpeg-3.1.2/coverage"

# CRETE_BIN_DIR=$GUEST_CRETE_BIN_DIR
# PARSEGCOVCMD=$CRETE_BIN_DIR/../../guest/scripts/replay/parse_gcov_coreutils.py
# PROG_DIR=$GUEST_PROG_DIR

PROGRAMS="ffmpeg
ffprobe"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETE_BIN_DIR
