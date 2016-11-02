REF_1=$1
REF_2=$2

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

# PROGRAMS="base64
# basename
# cat
# cksum
# comm
# cut
# date
# df
# dircolors"

for prog in $PROGRAMS
do
    diff -qr $REF_1/auto_$prog.xml/test-case/ $REF_2/auto_$prog.xml/test-case/ | grep diff | wc -l  | \
    { read diff_count; test $diff_count -ne 0 && printf "$prog\t\t$diff_count\n"; }
done
