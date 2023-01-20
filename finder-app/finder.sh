#!/bin/bash

function usage() {
    echo "Usage: $0 [filesdir] [searchstr]"
    echo "filesdir: path to a directory on the filesystem"
    echo "searchstr: a text string which will be searched in filesdir"
}

if [[ "$#" -ne 2 ]]; then
    usage
    exit 1
fi

filesdir=$1
searchstr=$2

if [[ -d $filesdir ]]; then
    # 'find $filesdir -type f | wc -l' to get number of files present
    # 'grep -r $searchstr $filesdir | wc -l' to get the number of files has searchstr
    echo "The number of files are $(find $filesdir -type f | wc -l) and the number of matching lines are $(grep -r $searchstr $filesdir | wc -l)"
else
    echo "$filesdir not found"
    exit 1
fi
