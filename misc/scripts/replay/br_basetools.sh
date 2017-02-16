# To generate branch coverage:
# editing your .lcovrc file (copied from /etc/lcovrc) to change lcov_branch_coverage setting to 1

# HOST_CRETE_BIN_DIR="/home/chenbo/crete/crete-dev/build/bin/"
# HOST_PROG_DIR="/home/chenbo/icse2016/eval/coreutils-6.10/crete-coverage/src"

# CRETE_BIN_DIR=$HOST_CRETE_BIN_DIR
# PARSEGCOVCMD=$CRETE_BIN_DIR/../../misc/scripts/replay/parse_gcov_coreutils.py
# PROG_DIR=$HOST_PROG_DIR

GUEST_CRETE_BIN_DIR="/home/test/guest-build/bin"
GUEST_PROG_DIR="/home/test/tests/tianocore-baseTools/Source/C/bin/"

CRETE_BIN_DIR=$GUEST_CRETE_BIN_DIR
PARSEGCOVCMD=$CRETE_BIN_DIR/../../guest/scripts/replay/parse_gcov_coreutils.py
PROG_DIR=$GUEST_PROG_DIR
LCOV_DIR=$GUEST_PROG_DIR/../

PROGRAMS="BootSectImage
EfiLdrImage
EfiRom
GenCrc32
GenFfs
GenFv
GenFw
GenPage
GenSec
GenVtf
GnuGenBootSector
LzmaCompress
Split
TianoCompress
VfrCompile
VolInfo"


export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETE_BIN_DIR
