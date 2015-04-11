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

import netscape.ldap.LDAPSortKey;

/**
 * Represents sorting instructions for a particular attribute.
 *
 */
public class LdapSortKey extends LDAPSortKey{

    /**
     * Constructs a new <CODE>LdapSortKey</CODE> object that will
     * sort based on the specified instructions.
     * @param keyDescription A single attribute specification to sort by.
     * If preceded by a hyphen ("-"), the attribute is sorted in reverse order.
     * You can also specify the object ID (OID) of a matching rule after
     * a colon (":"). For example:
     * <P>
     * <UL>
     * <LI><CODE>"cn"</CODE> (sort by the <CODE>cn</CODE> attribute) <P>
     * <LI><CODE>"-cn"</CODE> (sort by the <CODE>cn</CODE> attribute in
     * reverse order) <P>
     * <LI><CODE>"-cn:1.2.3.4"</CODE> (sort by the <CODE>cn</CODE>
     * attribute in reverse order and use the matching rule identified
     * by the OID 1.2.3.4) <P>
     *</UL>
     * @see com.netscape.jndi.ldap.controls.LdapSortControl
     * @see com.netscape.jndi.ldap.controls.LdapVirtualListControl
     */
    public LdapSortKey( String keyDescription ) {
        super(keyDescription);
    }

    /**
     * Constructs a new <CODE>LdapSortKey</CODE> object that will
     * sort based on the specified attribute and sort order.
     * @param key A single attribute to sort by.  For example:
     * <P>
     * <UL>
     * <LI><CODE>"cn"</CODE> (sort by the <CODE>cn</CODE> attribute)
     * <LI><CODE>"givenname"</CODE> (sort by the <CODE>givenname</CODE>
     * attribute)
     * </UL>
     * @param reverse If <CODE>true</CODE>, the sorting is done in
     * descending order.
     * @see com.netscape.jndi.ldap.controls.LdapSortControl
     * @see com.netscape.jndi.ldap.controls.LdapVirtualListControl
     */
    public LdapSortKey( String key,
                        boolean reverse) {
        super(key,reverse);
    }

    /**
     * Constructs a new <CODE>LdapSortKey</CODE> object that will
     * sort based on the specified attribute, sort order, and matching
     * rule.
     * @param key A single attribute to sort by. For example:
     * <P>
     * <UL>
     * <LI><CODE>"cn"</CODE> (sort by the <CODE>cn</CODE> attribute)
     * <LI><CODE>"givenname"</CODE> (sort by the <CODE>givenname</CODE>
     * attribute)
     * </UL>
     * @param reverse If <CODE>true</CODE>, the sorting is done in
     * descending order.
     * @param matchRule Object ID (OID) of the matching rule for
     * the attribute (for example, <CODE>1.2.3.4</CODE>).
     * @see com.netscape.jndi.ldap.controls.LdapSortControl
     * @see com.netscape.jndi.ldap.controls.LdapVirtualListControl
     */
    public LdapSortKey( String key,
                        boolean reverse,
                        String matchRule) {
        super(key, reverse, matchRule);
    }

    /**
     * Returns the attribute to sort by.
     * @return A single attribute to sort by.
     */
    public String getKey() {
        return super.getKey();
    }

    /**
     * Returns <CODE>true</CODE> if sorting is to be done in descending order.
     * @return <CODE>true</CODE> if sorting is to be done in descending order.
     */
    public boolean getReverse() {
        return super.getReverse();
    }

    /**
     * Returns the object ID (OID) of the matching rule used for sorting.
     * If no matching rule is specified, <CODE>null</CODE> is returned.
     * @return The object ID (OID) of the matching rule, or <CODE>null</CODE>
     * if the sorting instructions specify no matching rule.
     */
    public String getMatchRule() {
        return super.getMatchRule();
    }
}