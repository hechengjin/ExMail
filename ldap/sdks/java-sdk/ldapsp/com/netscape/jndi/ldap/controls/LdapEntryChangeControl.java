/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package com.netscape.jndi.ldap.controls;

import javax.naming.ldap.Control;
import netscape.ldap.controls.*;

/**
 * Represents an LDAP v3 server control that specifies information
 * about a change to an entry in the directory.  (The OID for this
 * control is 2.16.840.1.113730.3.4.7.)  You need to use this control in
 * conjunction with a "persistent search" control (represented
 * by <CODE>LdapPersistentSearchControl</CODE> object.
 * <P>
 *
 * To use persistent searching for change notification, you create a
 * "persistent search" control that specifies the types of changes that
 * you want to track.  When an entry is changed, the server sends that
 * entry back to your client and may include an "entry change notification"
 * control that specifies additional information about the change.
 * <P>
 *
 * Once you retrieve an <CODE>LdapEntryChangeControl</CODE> object from
 * the server, you can get the following additional information about
 * the change made to the entry:
 * <P>
 *
 * <UL>
 * <LI>The type of change made (add, modify, delete, or modify DN)
 * <LI>The change number identifying the record of the change in the
 * change log (if the server supports change logs)
 * <LI>If the entry was renamed, the old DN of the entry
 * </UL>
 * <P>
 *
 * @see com.netscape.jndi.ldap.controls.LdapPersistSearchControl
 */
public class LdapEntryChangeControl extends LDAPEntryChangeControl implements Control {

    /**
     * Constructs a new <CODE>LdapEntryChangeControl</CODE> object.
     * This constructor is used by the NetscapeControlFactory
     *
     */
    LdapEntryChangeControl(boolean critical, byte[] value) throws Exception {
        super(ENTRYCHANGED, critical, value);
    }

    /**
     * Gets the change number, which identifies the record of the change
     * in the server's change log.
     * @return Change number identifying the change made.
     */
    public int getChangeNumber() {
        return super.getChangeNumber();
    }

    /**
     * Gets the change type, which identifies the type of change
     * that occurred.
     * @return Change type identifying the type of change that
     * occurred.  This can be one of the following values:
     * <P>
     *
     * <UL>
     * <LI><CODE>LdapPersistSearchControl.ADD</CODE> (a new entry was
     * added to the directory)
     * <LI><CODE>LdapPersistSearchControl.DELETE</CODE> (an entry was
     * removed from the directory)
     * <LI><CODE>LdapPersistSearchControl.MODIFY</CODE> (an entry was
     * modified)
     * <LI><CODE>LdapPersistSearchControl.MODDN</CODE> (an entry was
     * renamed)
     * </UL>
     * <P>
     */
    public int getChangeType() {
        return super.getChangeType();
    }

    /**
     * Gets the previous DN of the entry (if the entry was renamed).
     * @return The previous distinguished name of the entry.
     */
    public String getPreviousDN() {
        return super.getPreviousDN();
    }

    /**
     * Retrieves the ASN.1 BER encoded value of the LDAP control.
     * Null is returned if the value is absent.
     * @return A possibly null byte array representing the ASN.1 BER
     * encoded value of the LDAP control.
     */
    public byte[] getEncodedValue() {
        return getValue();
    }    
}

