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
package com.netscape.jndi.ldap.schema;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;

import netscape.ldap.*;
import netscape.ldap.controls.*;

import java.util.*;

public class SchemaElement extends SchemaDirContext {

    SchemaManager m_schemaMgr;
    
    // Attributes used to define schema elements
    static final String NUMERICOID = "NUMERICOID";
    static final String NAME = "NAME";
    static final String DESC = "DESC";
    static final String SYNTAX = "SYNTAX";
    static final String SUP = "SUP";
    static final String MUST = "MUST";
    static final String MAY = "MAY";    
    static final String SINGLEVALUE = "SINGLE-VALUE";
        
    // These attribute definition properties are  not supported by LdapJDK and DS
    // Accept them for the appropriate schema element, but ignore them
    static final String OBSOLETE = "OBSOLETE";    
    static final String EQUALITY = "EQUALITY";
    static final String ORDERING = "ORDERING";
    static final String SUBSTRING = "SUBSTRING";
    static final String COLLECTIVE = "COLLECTIVE";
    static final String NOUSERMOD = "NO-USER-MODIFICATION";
    static final String USAGE = "USAGE";
    static final String ABSTRACT = "ABSTRACT";
    static final String STRUCTURAL = "STRUCTURAL";
    static final String AUXILIARY = "AUXILIARY";
    

    // Syntax string recognized by Netscape LdapJDK
    static final String cisString       = "1.3.6.1.4.1.1466.115.121.1.15";
    static final String binaryString    = "1.3.6.1.4.1.1466.115.121.1.5";
    static final String telephoneString = "1.3.6.1.4.1.1466.115.121.1.50";
    static final String cesString       = "1.3.6.1.4.1.1466.115.121.1.26";
    static final String intString       = "1.3.6.1.4.1.1466.115.121.1.27";
    static final String dnString        = "1.3.6.1.4.1.1466.115.121.1.12";
    
    SchemaElement(SchemaManager schemaMgr) {
        m_schemaMgr = schemaMgr;
    }    
    
    /**
     * Map a syntax oid string to a constant recognized by LdapJDK
     */
    static int syntaxStringToInt(String syntax) throws NamingException{
        if (syntax.equals(cisString)) {
            return LDAPSchemaElement.cis;
        }
        else if (syntax.equals(cesString)) {
            return LDAPSchemaElement.ces;
        }
        else if (syntax.equals(telephoneString)) {
            return LDAPSchemaElement.telephone;
        }
        else if (syntax.equals(intString)) {
            return LDAPSchemaElement.integer;
        }
        else if (syntax.equals(dnString)) {
            return LDAPSchemaElement.dn;
        }
        else if (syntax.equals(binaryString)) {
            return LDAPSchemaElement.binary;
        }
        else {
            throw new InvalidAttributeValueException(syntax);
        }    
    }    

    /**
     * Map a syntax identifier to a oid string
     */
    static String syntaxIntToString(int syntax) throws NamingException{
        if (syntax == LDAPSchemaElement.cis) {
            return cisString;
        }
        else if (syntax == LDAPSchemaElement.ces) {
            return cesString;
        }
        else if (syntax == LDAPSchemaElement.telephone) {
            return telephoneString;
        }
        else if (syntax == LDAPSchemaElement.integer) {
            return intString;
        }
        else if (syntax == LDAPSchemaElement.dn) {
            return dnString;
        }
        else if (syntax == LDAPSchemaElement.binary) {
            return binaryString;
        }
        else {
            throw new InvalidAttributeValueException("Interanal error, unexpected syntax value " + syntax);
        }    
    }    

    /**
     * Convert string vector to an array
     */
    static String[] vectorToStringAry(Vector v) {
        String[] ary = new String[v.size()];
        for(int i=0; i<v.size(); i++) {
            ary[i] = (String) v.elementAt(i);
        }
        return ary;
    }

    /**
     * List Operations
     */

    public NamingEnumeration list(String name) throws NamingException {
        return new EmptyNamingEnumeration();
    }

    public NamingEnumeration list(Name name) throws NamingException {
        return new EmptyNamingEnumeration();
    }

    public NamingEnumeration listBindings(String name) throws NamingException {
        return new EmptyNamingEnumeration();
    }

    public NamingEnumeration listBindings(Name name) throws NamingException {
        return new EmptyNamingEnumeration();
    }

    /**
     * Modify the current set of the schema element's attributes
     */
    void modifySchemaElementAttrs (Attributes attrs, ModificationItem[] jndiMods) throws NamingException{
        LDAPModificationSet mods = new LDAPModificationSet();
        for (int i=0; i < jndiMods.length; i++) {
            int modop = jndiMods[i].getModificationOp();
            Attribute attr = jndiMods[i].getAttribute();
            Attributes modAttrs = new BasicAttributes(/*ignorecase=*/true);
            modAttrs.put(attr);
            modifySchemaElementAttrs(attrs, modop, modAttrs);
        }
    }

    /**
     * Modify the current set of the schema element's attributes
     */
    void modifySchemaElementAttrs (Attributes attrs, int modop, Attributes modAttrs) throws NamingException{
        LDAPModificationSet mods = new LDAPModificationSet();
        for (NamingEnumeration attrEnum = modAttrs.getAll(); attrEnum.hasMore();) {
            Attribute attr = (Attribute)attrEnum.next();
            if (modop == DirContext.ADD_ATTRIBUTE) {
                Attribute curAttr = attrs.get(attr.getID());
                if (curAttr == null) {
                    attrs.put(attr);
                }
                else {
                    for (NamingEnumeration vals = attr.getAll(); vals.hasMore();) {
                        curAttr.add(vals.nextElement());
                    }
                }
            }
            else if (modop == DirContext.REPLACE_ATTRIBUTE) {
                attrs.put(attr);
            }    
            else if (modop == DirContext.REMOVE_ATTRIBUTE) {
                Attribute curAttr = attrs.get(attr.getID());
                if (curAttr == null) {
                    throw new NoSuchAttributeException(attr.getID());
                }
                else if (attr.size() == 0) { // remove the attr
                    attrs.remove(attr.getID());
                }    
                else {
                    for (NamingEnumeration vals = attr.getAll(); vals.hasMore();) {
                        String val = (String) vals.nextElement();
                        curAttr.remove(val);
                        // Schema definition Values are case insensitive 
                        curAttr.remove(val.toLowerCase());
                    }
                    if (curAttr.size() == 0) { // remove attr if no values left
                        attrs.remove(attr.getID());
                    }    
                }                
            }
            else {
                throw new IllegalArgumentException("Illegal Attribute Modification Operation");
            }
        }
    }         

    /**
     * Parse value for a schema attribute. Return true if the value is
     * "true", return false if the value is "false" or absent
     */
    static boolean parseTrueFalseValue(Attribute attr) throws NamingException{
        
        for (NamingEnumeration valEnum = attr.getAll(); valEnum.hasMore(); ) {
            String flag = (String)valEnum.nextElement();
            if (flag.equals("true")) {
                return true;
            }
            else if (flag.equals("false")) {
                return false;
            }
            else {
                throw new InvalidAttributeValueException(attr.getID() +
                          " value must be \"true\", \"false\" or absent");
            }
        }
        return false; // no values
    }
    
    /**
     * Read a string value for a schema attribute
     */
    static String getSchemaAttrValue(Attribute attr) throws NamingException {
        for (Enumeration valEnum = attr.getAll(); valEnum.hasMoreElements(); ) {
            return (String)valEnum.nextElement();
        }
        throw new InvalidAttributeValueException(attr.getID() + " must have a value");
    }
}
