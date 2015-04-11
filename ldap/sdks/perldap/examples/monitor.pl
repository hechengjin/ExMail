#!/usr/bin/perl5
#############################################################################
# $Id: monitor.pl,v 1.4 2007/06/19 11:27:05 gerv%gerv.net Exp $
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is PerLDAP.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corp.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Clayton Donley
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# DESCRIPTION
#    Ask the directory server for it's monitor entry, to see some
#    performance and usage stats.

use Getopt::Std;			# To parse command line arguments.
use Mozilla::LDAP::Conn;		# Main "OO" layer for LDAP
use Mozilla::LDAP::Utils;		# LULU, utilities.


#################################################################################
# Constants, shouldn't have to edit these...
#
$APPNAM	= "monitor";
$USAGE	= "$APPNAM [-nv] -b base -h host -D bind -w pswd -P cert";


#################################################################################
# Check arguments, and configure some parameters accordingly..
#
if (!getopts('b:h:D:p:w:P:'))
{
   print "usage: $APPNAM $USAGE\n";
   exit;
}
%ld = Mozilla::LDAP::Utils::ldapArgs("", "cn=monitor");


#################################################################################
# Instantiate an LDAP object, which also binds to the LDAP server, and then
# do the simple search.
#
$conn = new Mozilla::LDAP::Conn(\%ld);
die "Could't connect to LDAP server $ld{host}" unless $conn;

$entry = $conn->search($ld{root}, "base", "objectclass=*");
Mozilla::LDAP::Utils::printEntry($entry)
  if ($entry);


#################################################################################
# Close the connection.
#
$conn->close if $conn;
