#!/bin/sh

objdir=""
if [ "$1" = "" ]; then
	objdir="$PWD/../mozilla"
else
	objdir="$PWD/../$1"
fi

if [ ! -d "$objdir" ]; then
	echo "$objdir does not exist."
	exit 0
fi







cd $objdir/tools/update-packaging
./make_full_update.sh