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
package netscape.ldap.client;

import java.util.*;
import netscape.ldap.ber.stream.*;
import java.io.*;

/**
 * This class implements the filter present.
 * See RFC 1777.
 * <pre>
 *   present [7] AttributeType
 * </pre>
 *
 * @version 1.0
 */
public class JDAPFilterPresent extends JDAPFilter {
    /**
     * Internal variables
     */
    private String m_type = null;

    /**
     * Constructs the filter.
     * @param type attribute type
     */
    public JDAPFilterPresent(String type) {
        super();
        m_type = type;
    }

    /**
     * Gets ber representation of the filter.
     * @return ber representation
     */
    public BERElement getBERElement() {
        BEROctetString s = new BEROctetString(m_type);
        BERTag element = new BERTag(BERTag.CONTEXT|7, s, true);
        return element;
    }

    /**
     * Gets string reprensetation of the filter.
     * @return string representation
     */
    public String toString() {
        return "JDAPFilterPresent {" + m_type + "}";
    }
}
