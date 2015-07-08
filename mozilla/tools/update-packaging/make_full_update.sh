#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
# This tool generates full update packages for the update system.
# Author: Darin Fisher
#

. $(dirname "$0")/common.sh

# -----------------------------------------------------------------------------

print_usage() {
  notice "Usage: $(basename $0) [OPTIONS] ARCHIVE DIRECTORY"
}


# -----------------------------------------------------------------------------

#从cfgfile中读取obj目录，根据目录位置，查找mar.exe位置，指定complete.mar生成位置和目标文件夹位置
CFGFILENAME="$(dirname "$0")/updatecfg"

echo cfg file path:${CFGFILENAME}
count=1

exec 3<> $CFGFILENAME
while read LINE <&3
do {

	if [ $count -gt 1 ]; then
		break
	fi

	count=$[ $count + 1 ]
	
	srcdir=$LINE
}
done
exec 3>&-

targetdir=${srcdir}/installer-stage/core
archive=${srcdir}/installer-stage/update.mar
mardir=${srcdir}/mozilla/dist/host/bin/mar.exe

echo targetdir:${srcdir}
echo archive:${archive}
echo mardir:${mardir}

#----------------------------------------------------------------------------------


# Prevent the workdir from being inside the targetdir so it isn't included in
# the update mar.
if [ $(echo "$targetdir" | grep -c '\/$') = 1 ]; then
  # Remove the /
  targetdir=$(echo "$targetdir" | sed -e 's:\/$::')
fi
workdir="$targetdir.work"
updatemanifestv1="$workdir/update.manifest"
updatemanifestv2="$workdir/updatev2.manifest"
targetfiles="update.manifest updatev2.manifest"

mkdir -p "$workdir"

# On Mac, the precomplete file added by Bug 386760 will cause OS X to reload the
# Info.plist so it launches the right architecture, bug 600098

# Generate a list of all files in the target directory.
pushd "$targetdir"
if test $? -ne 0 ; then
  exit 1
fi

if [ ! -f "precomplete" ]; then
  notice "precomplete file is missing!"
  exit 1
fi

list_files files

popd

notice ""
notice "Adding file add instructions to file 'update.manifest'"
> $updatemanifestv1

num_files=${#files[*]}

for ((i=0; $i<$num_files; i=$i+1)); do
  f="${files[$i]}"

  make_add_instruction "$f" >> $updatemanifestv1

  dir=$(dirname "$f")
  mkdir -p "$workdir/$dir"
  $BZIP2 -cz9 "$targetdir/$f" > "$workdir/$f"
  copy_perm "$targetdir/$f" "$workdir/$f"

  targetfiles="$targetfiles \"$f\""
done

# Add the type of update to the beginning of and cat the contents of the version
# 1 update manifest to the version 2 update manifest.
> $updatemanifestv2
notice ""
notice "Adding type instruction to file 'updatev2.manifest'"
notice "       type: complete"
echo "type \"complete\"" >> $updatemanifestv2

notice ""
notice "Concatenating file 'update.manifest' to file 'updatev2.manifest'"
cat $updatemanifestv1 >> $updatemanifestv2

# Append remove instructions for any dead files.
notice ""
notice "Adding file and directory remove instructions from file 'removed-files'"
append_remove_instructions "$targetdir" "$updatemanifestv1" "$updatemanifestv2"

$BZIP2 -z9 "$updatemanifestv1" && mv -f "$updatemanifestv1.bz2" "$updatemanifestv1"
$BZIP2 -z9 "$updatemanifestv2" && mv -f "$updatemanifestv2.bz2" "$updatemanifestv2"

eval "$mardir -C \"$workdir\" -c output.mar $targetfiles"
mv -f "$workdir/output.mar" "$archive"

# cleanup
rm -fr "$workdir"

notice ""
notice "Finished"

echo "--------------------------"
echo "filesize:"
ls -al $archive | awk '{print $5}'
echo "--------------------------"
echo "sha512:"
openssl dgst -sha512 $archive | awk '{print $2}'
echo "--------------------------"