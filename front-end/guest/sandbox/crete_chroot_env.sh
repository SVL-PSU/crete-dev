#!/bin/bash

# Buiding chroot environment for the program under test.
#
# This script uses `ldd' to get the shared libraries required by a program,
# and copies those libraries to the directory for chroot'ing, maintaining
# each library's relative path against root directory.
#
# Usage example:
# $ ./crete-chroot-env /tmp/sandbox `which rm` # build env for 'rm'
# $ sudo setcap CAP_SYS_CHROOT+ep `which crete-run`
#              # give crete-run the capability to call 'chroot' system call

if [ $# != 2 ] ; then
    echo "usage $0 PATH_TO_BINARY TARGET_FOLDER"
    exit 1
fi

PATH_TO_BINARY="$1"
TARGET_FOLDER="$2"

# if we cannot find the the binary we have to abort
if [ ! -f "$PATH_TO_BINARY" ] ; then
    echo "The file '$PATH_TO_BINARY' was not found. Aborting!"
    exit 1
fi

# copy the binary to the target folder
# create directories if required
echo "---> copy binary itself"
cp --parents -v "$PATH_TO_BINARY" "$TARGET_FOLDER"

# copy the required shared libs to the target folder
# create directories if required
echo "---> copy libraries"
for lib in `ldd "$PATH_TO_BINARY" | cut -d'>' -f2 | awk '{print $1}'` ; do
   if [ -f "$lib" ] ; then
        cp -v --parents "$lib" "$TARGET_FOLDER"
   fi
done

# I'm on a 64bit system at home. the following code will be not required on a 32bit system.
# however, I've not tested that yet
# create lib64 - if required and link the content from lib to it
if [ ! -d "$TARGET_FOLDER/lib64" ] ; then
    mkdir -v "$TARGET_FOLDER/lib64"
fi
