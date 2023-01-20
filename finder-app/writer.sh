#!/bin/bash

function usage() {
    echo "Usage: $0 [writefile] [writestr]"
    echo "writefile: full path to a file (including filename) on the filesystem"
    echo "writestr: a text string which will be written within this file"
}

if [[ "$#" -ne 2 ]]; then
    usage
    exit 1
fi

writefile=$1
writestr=$2

# strip dirname from absolute filepath
dir=$(dirname "$writefile")

# create directory if it doesn't exists
if [[ ! -f $writefile ]]; then
    mkdir -p $dir
fi

# write string to the file
echo $writestr > $writefile
