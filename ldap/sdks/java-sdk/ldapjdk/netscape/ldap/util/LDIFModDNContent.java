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
package netscape.ldap.util;

/**
 * An object of this class represents the content of an LDIF record that
 * specifies changes to an RDN or the DN of an entry.  This class
 * implements the <CODE>LDIFContent</CODE> interface.
 * <P>
 *
 * To get this object from an <CODE>LDIFRecord</CODE> object,
 * use the <CODE>getContent</CODE> method and cast the return value as
 * <CODE>LDIFModDNContent</CODE>.
 * <P>
 *
 * @version 1.0
 * @see netscape.ldap.util.LDIFRecord#getContent
 */
public class LDIFModDNContent extends LDIFBaseContent {
    /**
     * Internal variables
     */
    private String m_newParent = null;
    private String m_rdn = null;
    private boolean m_deleteOldRDN = false;
    static final long serialVersionUID = 1352504898614557791L;

    /**
     * Constructs an empty <CODE>LDIFModDNContent</CODE> object.
     * To specify the modifications to be made to the entry, use
     * the <CODE>setRDN</CODE>, <CODE>setNewParent</CODE>,
     * and <CODE>setDeleteOldRDN</CODE> methods.
     * @see netscape.ldap.util.LDIFModDNContent#setRDN
     * @see netscape.ldap.util.LDIFModDNContent#setNewParent
     * @see netscape.ldap.util.LDIFModDNContent#setDeleteOldRDN
     */
    public LDIFModDNContent() {
    }

    /**
     * Returns the content type. You can use this with the
     * <CODE>getContent</CODE> method of the <CODE>LDIFRecord</CODE>
     * object to determine the type of content specified in the record.
     * @return the content type (which is
     * <CODE>LDIFContent.MODDN_CONTENT</CODE>).
     * @see netscape.ldap.util.LDIFRecord#getContent
     */
    public int getType() {
        return MODDN_CONTENT;
    }

    /**
     * Sets the new RDN that should be assigned to the entry.
     * @param rdn the new RDN
     * @see netscape.ldap.util.LDIFModDNContent#getRDN
     */
    public void setRDN(String rdn) {
        m_rdn = rdn;
    }

    /**
     * Returns the new RDN specified in the content of the LDIF record.
     * @return the new RDN.
     * @see netscape.ldap.util.LDIFModDNContent#setRDN
     */
    public String getRDN() {
        return m_rdn;
    }

    /**
     * Sets the new parent DN that should be assigned to the entry.
     * @param parent the new parent DN for the entry
     * @see netscape.ldap.util.LDIFModDNContent#getNewParent
     */
    public void setNewParent(String parent) {
        m_newParent = parent;
    }

    /**
     * Returns the entry's new parent DN, if specified in the content
     * of the LDIF record.
     * @return the new parent of the entry.
     * @see netscape.ldap.util.LDIFModDNContent#setNewParent
     */
    public String getNewParent() {
        return m_newParent;
    }

    /**
     * Sets whether or not the old RDN should be removed as an
     * attribute in the entry.
     * @param bool if <CODE>true</CODE>, remove the attribute representing
     * the RDN.  If <CODE>false</CODE>, leave the attribute in the entry.
     * @see netscape.ldap.util.LDIFModDNContent#getDeleteOldRDN
     */
    public void setDeleteOldRDN(boolean bool) {
        m_deleteOldRDN = bool;
    }

    /**
     * Determines if the content of the LDIF record specifies that
     * the old RDN should be removed as an attribute in the entry.
     * @return <CODE>true</CODE> if the change specifies that the
     * the attribute representing the RDN should be removed,
     * <CODE>false</CODE> if the change specifies that the attribute
     * should be left in the entry.
     * @see netscape.ldap.util.LDIFModDNContent#setDeleteOldRDN
     */
    public boolean getDeleteOldRDN() {
        return m_deleteOldRDN;
    }

    /**
     * Returns string representation of the content of the LDIF record.
     * @return the string representation of the content of the LDIF record.
     */
    public String toString() {
        String s = "";
        if (m_newParent == null)
            s = s + "new parent() ";
        else
            s = s + "new parent( "+m_newParent+" ), ";
        if (m_deleteOldRDN)
            s = s + "deleteOldRDN( true ), ";
        else
            s = s + "deleteOldRDN( false ), ";
        if (m_rdn == null)
            s = s + "new rdn()";
        else
            s = s + "new rdn( "+m_rdn+" )";

        if ( getControls() != null ) {
            s += getControlString();
        }
        return "LDIFModDNContent {" + s + "}";
    }
}
