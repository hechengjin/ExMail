#!/bin/sh

SIGNTOOL=./tools/cmd-sign/signtool.exe
PFXPATH=./SecurityCertificate/wyyt.pfx
PASSWORD=Firemail
TIMESTAMP=http://timestamp.comodoca.com/authenticode
PROPATH=./signfiles/

echo "--------------------------sign other begin---------------------"
echo ""
	setupName=$(find $PROPATH -name "*.*")
	$SIGNTOOL sign //f $PFXPATH //p $PASSWORD //t $TIMESTAMP $setupName
echo ""
echo "--------------------------sign otherl end-----------------------"

