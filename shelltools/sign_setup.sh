#!/bin/sh

objdirname=""
if [ "$1" = "" ]; then
	objdirname="obj_Firemail"
else
	objdirname="$1"
fi



SIGNTOOL=./tools/cmd-sign/signtool.exe
PFXPATH=./SecurityCertificate/wyyt.pfx
PASSWORD=Firemail
TIMESTAMP=http://timestamp.comodoca.com/authenticode
SETUPPATH=../$objdirname/mozilla/dist/install/sea


echo "--------------------------sign setup.exe begin---------------------"
echo ""
	setupName=$(find $SETUPPATH -name "*installer.exe")
	$SIGNTOOL sign //f $PFXPATH //p $PASSWORD  $setupName
echo ""
echo "--------------------------sign setup.exe end-----------------------"
