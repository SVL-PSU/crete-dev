# To generate branch coverage:
# editing your .lcovrc file (copied from /etc/lcovrc) to change lcov_branch_coverage setting to 1

# HOST_CRETE_BIN_DIR="/home/chenbo/crete/crete-dev/build/bin/"
# HOST_PROG_DIR="/home/chenbo/icse2016/eval/coreutils-6.10/crete-coverage/src"

# CRETE_BIN_DIR=$HOST_CRETE_BIN_DIR
# PARSEGCOVCMD=$CRETE_BIN_DIR/../../misc/scripts/replay/parse_gcov_coreutils.py
# PROG_DIR=$HOST_PROG_DIR

GUEST_CRETE_BIN_DIR="/home/test/guest-build/bin"
GUEST_PROG_DIR="/home/test/tests/coreutils-6.10/coverage/src/"

CRETE_BIN_DIR=$GUEST_CRETE_BIN_DIR
PARSEGCOVCMD=$CRETE_BIN_DIR/../../guest/scripts/replay/parse_gcov_coreutils.py
PROG_DIR=$GUEST_PROG_DIR
LCOV_DIR=$GUEST_PROG_DIR/../
SANDBOX="/home/test/klee-sandbox"

# ======================================
# complete list
# ======================================
PROGRAMS="base64
basename
cat
chcon
chgrp
chmod
chown
chroot
cksum
comm
cp
csplit
cut
date
dd
df
dircolors
dirname
du
echo
env
expand
expr
factor
false
fmt
fold
head
hostid
hostname
id
ginstall
join
kill
link
ln
logname
ls
md5sum
mkdir
mkfifo
mknod
mktemp
mv
nice
nl
nohup
od
paste
pathchk
pinky
pr
printenv
printf
ptx
pwd
readlink
rm
rmdir
runcon
seq
setuidgid
shred
shuf
sleep
sort
split
stat
stty
sum
sync
tac
tail
tee
touch
tr
tsort
tty
uname
unexpand
uniq
unlink
uptime
users
wc
whoami
who
yes"

# ======================================
# crete old 49 progs
# ======================================
# PROGRAMS="base64
# basename
# cat
# cksum
# comm
# cut
# date
# df
# dircolors
# dirname
# echo
# env
# expand
# expr
# factor
# fmt
# fold
# head
# hostid
# id
# join
# logname
# ls
# nl
# od
# paste
# pathchk
# pinky
# printenv
# printf
# pwd
# readlink
# seq
# shuf
# sleep
# sort
# stat
# sum
# sync
# tac
# tr
# tsort
# uname
# unexpand
# uptime
# users
# wc
# whoami
# who"

# ======================================
# selected ones
# ======================================
# PROGRAMS="base64
# basename"


export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CRETE_BIN_DIR
