#!/bin/sh

objdir=""
if [ "$1" = "" ]; then
	objdir="$PWD/../obj_Firemail"
else
	objdir="$PWD/../$1"
fi

if [ ! -d "$objdir" ]; then
	echo "$objdir does not exist."
	exit 0
fi


cd $objdir/mail/installer/windows
make uninstaller

cd $objdir
make installer


