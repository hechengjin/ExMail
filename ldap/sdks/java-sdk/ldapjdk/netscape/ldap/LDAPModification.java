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
package netscape.ldap;

import netscape.ldap.ber.stream.*;

/**
 * Specifies changes to be made to the values of an attribute.  The change is
 * specified in terms of the following aspects:
 * <P>
 *
 * <UL>
 * <LI>the type of modification (add, replace, or delete the value of an attribute)
 * <LI>the type of value being modified (string or binary)
 * <LI>the name of the attribute being modified
 * <LI>the actual value
 * </UL>
 * <P>
 *
 * After you specify a change to an attribute, you can execute the change
 * by calling the <CODE>LDAPConnection.modify</CODE> method and specifying
 * the DN of the entry that you want to modify.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPConnection#modify(java.lang.String, netscape.ldap.LDAPModification)
 */
public class LDAPModification implements java.io.Serializable {

    static final long serialVersionUID = 4836472112866826595L;

    /**
     * Specifies that a value should be added to an attribute.
     */
    public static final int ADD     = 0;

    /**
     * Specifies that a value should be removed from an attribute.
     */
    public static final int DELETE  = 1;

    /**
     * Specifies that a value should replace the existing value in an attribute.
     */
    public static final int REPLACE = 2;

    /**
     * Internal variables
     */
    private int operation;
    private LDAPAttribute attribute;

    /**
     * Specifies a modification to be made to an attribute.
     * @param op the type of modification to make. This can be one of the following:
     *   <P>
     *   <UL>
     *   <LI><CODE>LDAPModification.ADD</CODE> (the value should be added to the attribute)
     *   <LI><CODE>LDAPModification.DELETE</CODE> (the value should be removed from the attribute)
     *   <LI><CODE>LDAPModification.REPLACE</CODE> (the value should replace the existing value of the attribute)
     *   </UL><P>
     * @param attr the attribute (possibly with values) to modify
     * @see netscape.ldap.LDAPAttribute
     */
    public LDAPModification( int op, LDAPAttribute attr ) {
        operation = op;
        attribute = attr;
    }

    /**
     * Returns the type of modification specified by this object.
     * @return one of the following types of modifications:
     *   <P>
     *   <UL>
     *   <LI><CODE>LDAPModification.ADD</CODE> (the value should be added to the attribute)
     *   <LI><CODE>LDAPModification.DELETE</CODE> (the value should be removed from the attribute)
     *   <LI><CODE>LDAPModification.REPLACE</CODE> (the value should replace the existing value of the attribute)
     *   </UL><P>
     */
    public int getOp() {
        return operation;
    }

    /**
     * Returns the attribute (possibly with values) to be modified.
     * @return the attribute to be modified.
     * @see netscape.ldap.LDAPAttribute
     */
    public LDAPAttribute getAttribute() {
        return attribute;
    }

    /**
     * Retrieves the BER (Basic Encoding Rules) representation
     * of the current modification.
     * @return BER representation of the modification.
     */
    public BERElement getBERElement() {
        BERSequence seq = new BERSequence();
        seq.addElement(new BEREnumerated(operation));
        seq.addElement(attribute.getBERElement());
        return seq;
    }

    /**
     * Retrieves the string representation of the current
     * modification. For example:
     *
     * <PRE>
     * LDAPModification: REPLACE, LDAPAttribute {type='mail', values='babs@ace.com'}
     * LDAPModification: ADD, LDAPAttribute {type='description', values='This entry was modified with the modattrs program'}
     * </PRE>
     *
     * @return string representation of the current modification.
     */
    public String toString() {
        String s = "LDAPModification: ";
        if ( operation == ADD )
            s += "ADD, ";
        else if ( operation == DELETE )
            s += "DELETE, ";
        else if ( operation == REPLACE )
            s += "REPLACE, ";
        else
            s += "INVALID OP, ";
        s += attribute;
        return s;
    }
}
