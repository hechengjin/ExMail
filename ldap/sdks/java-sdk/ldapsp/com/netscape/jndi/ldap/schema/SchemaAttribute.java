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

public class SchemaAttribute extends SchemaElement {

    LDAPAttributeSchema m_ldapAttribute;
    
    // Attribute IDs that are exposed through Netscape LdapJDK
    private static String[] m_allAttrIds = {NUMERICOID, NAME, DESC, OBSOLETE,
                                            SUP, EQUALITY, ORDERING, SUBSTRING,
                                            SYNTAX, SINGLEVALUE, COLLECTIVE,
                                            NOUSERMOD, USAGE
    };
    
    public SchemaAttribute(LDAPAttributeSchema ldapAttribute, SchemaManager schemaManager) {
        super(schemaManager);
        m_ldapAttribute = ldapAttribute;
        m_path = ATTRDEF + "/" + m_ldapAttribute.getName();
    }

    public SchemaAttribute(Attributes attrs,  SchemaManager schemaManager) throws NamingException {
        super(schemaManager);        
        m_ldapAttribute = parseDefAttributes(attrs);
        m_path = ATTRDEF + "/" + m_ldapAttribute.getName();
    }    
    
    /**
     * Parse Definition Attributes for a LDAP attribute
     */
    static LDAPAttributeSchema parseDefAttributes(Attributes attrs) throws NamingException {        
        String name=null, oid=null, desc=null, syntax=null, usage=null, sup=null;
        String equality=null, ordering=null, substring=null;
        boolean singleValued=false, collective=false, obsolete=false, noUserMod=false;

        for (Enumeration attrEnum = attrs.getAll(); attrEnum.hasMoreElements(); ) {
            Attribute attr = (Attribute) attrEnum.nextElement();
            String attrName = attr.getID();
            if (attrName.equals(NAME)) {
                name = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(NUMERICOID)) {
                oid = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(SYNTAX)) {
                syntax = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(DESC)) {
                desc = getSchemaAttrValue(attr);
            }            
            else if (attrName.equals(SINGLEVALUE)) {
                singleValued = parseTrueFalseValue(attr);
            }            
            else if (attrName.equals(SUP)) {
                sup = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(USAGE)) {
                usage = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(OBSOLETE)) {
                obsolete = parseTrueFalseValue(attr);
            }
            else if (attrName.equals(COLLECTIVE)) {
                collective = parseTrueFalseValue(attr);
            }
            else if (attrName.equals(NOUSERMOD)) {
                noUserMod = parseTrueFalseValue(attr);
            }
            else if (attrName.equals(EQUALITY)) {
                equality = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(ORDERING)) {
                ordering = getSchemaAttrValue(attr);
            }
            else if (attrName.equals(SUBSTRING)) {
                substring = getSchemaAttrValue(attr);
            }

            else { 
                throw new NamingException("Invalid schema attribute type for attribute definition "    + attrName);
            }
        }    
            
        LDAPAttributeSchema attrSchema = new LDAPAttributeSchema(name, oid, desc, syntax, singleValued, sup, null);
        
        if (obsolete) {
            attrSchema.setQualifier(OBSOLETE, "");
        }            
        if (collective) {
            attrSchema.setQualifier(COLLECTIVE, "");
        }            
        if (noUserMod) {
            attrSchema.setQualifier(NOUSERMOD, "");
        }            
        if (equality != null) {
            attrSchema.setQualifier(EQUALITY, equality);
        }            
        if (ordering != null) {
            attrSchema.setQualifier(ORDERING, ordering);
        }            
        if (substring != null) {
            attrSchema.setQualifier(SUBSTRING, substring);
        }            
        if (usage != null) {
            attrSchema.setQualifier(USAGE, usage);
        } 
        
        return attrSchema;
    }
    
        
    /**
     * Exctract specified attributes from the ldapObjectClass
     */
    Attributes extractAttributeIds(String[] attrIds) throws NamingException{
        Attributes attrs = new BasicAttributes();
        String val = null, multiVal[]=null;
        for (int i = 0; i < attrIds.length; i++) {
            if (attrIds[i].equals(NUMERICOID)) {
                val = m_ldapAttribute.getID();
                if (val != null) {
                    attrs.put(new BasicAttribute(NUMERICOID, val));
                }                    
            }
            else if (attrIds[i].equals(NAME)) {
                val = m_ldapAttribute.getName();
                if (val != null) {
                    attrs.put(new BasicAttribute(NAME, val));
                }                    
            }
            else if (attrIds[i].equals(DESC)) {
                val = m_ldapAttribute.getDescription();
                if (val != null) {
                    attrs.put(new BasicAttribute(DESC, val));
                }                    
            }
            else if (attrIds[i].equals(SYNTAX)) {
                val = m_ldapAttribute.getSyntaxString();
                if (val != null) {
                    attrs.put(new BasicAttribute(SYNTAX, val));
                }                    
            }
            else if (attrIds[i].equals(SINGLEVALUE)) {
                if (m_ldapAttribute.isSingleValued()) {
                    attrs.put(new BasicAttribute(SINGLEVALUE, "true"));
                }
            }
            else if (attrIds[i].equals(SUP)) {
                val = m_ldapAttribute.getSuperior();
                if (val != null) {
                    attrs.put(new BasicAttribute(SUP, val));
                }                    
            }
            else if (attrIds[i].equals(USAGE)) {
                multiVal = m_ldapAttribute.getQualifier(USAGE);
                if (multiVal != null) {
                    attrs.put(new BasicAttribute(USAGE, multiVal));
                }                    
            }
            else if (attrIds[i].equals(OBSOLETE)) {
                if (m_ldapAttribute.getQualifier(OBSOLETE) != null) {
                    attrs.put(new BasicAttribute(OBSOLETE, "true"));
                }
            }
            else if (attrIds[i].equals(COLLECTIVE)) {
                if (m_ldapAttribute.getQualifier(COLLECTIVE) != null) {
                    attrs.put(new BasicAttribute(COLLECTIVE, "true"));
                }
            }
            else if (attrIds[i].equals(NOUSERMOD)) {
                if (m_ldapAttribute.getQualifier(NOUSERMOD) != null) {
                    attrs.put(new BasicAttribute(NOUSERMOD, "true"));
                }
            }
            else if (attrIds[i].equals(EQUALITY)) {
                multiVal = m_ldapAttribute.getQualifier(EQUALITY);
                if (multiVal != null) {
                    attrs.put(new BasicAttribute(EQUALITY, multiVal));
                }
            }
            else if (attrIds[i].equals(ORDERING)) {
                multiVal = m_ldapAttribute.getQualifier(ORDERING);
                if (multiVal != null) {
                    attrs.put(new BasicAttribute(ORDERING, multiVal));
                }
            }
            else if (attrIds[i].equals(SUBSTRING)) {
                multiVal = m_ldapAttribute.getQualifier(SUBSTRING);
                if (multiVal != null) {
                    attrs.put(new BasicAttribute(SUBSTRING, multiVal));
                }
            }
            else { 
                throw new NamingException("Invalid schema attribute type for attribute definition "    + attrIds[i]);
            }
        }
        return attrs;
    }    
        
        
    /**
     * DirContext Attribute Operations
     */

    public Attributes getAttributes(String name) throws NamingException {
        if (name.length() != 0) { // no subcontexts
            throw new NameNotFoundException(name); // no such object
        }
        return extractAttributeIds(m_allAttrIds);
    }

    public Attributes getAttributes(String name, String[] attrIds) throws NamingException {
        if (name.length() != 0) { // no subcontexts
            throw new NameNotFoundException(name); // no such object
        }
        return extractAttributeIds(attrIds);
    }

    public Attributes getAttributes(Name name) throws NamingException {
        return getAttributes(name.toString());
    }

    public Attributes getAttributes(Name name, String[] attrIds) throws NamingException {
        return getAttributes(name.toString(), attrIds);
    }

    public void modifyAttributes(String name, int mod_op, Attributes attrs) throws NamingException {
        if (name.length() != 0) { // no subcontexts
            throw new NameNotFoundException(name); // no such object
        }
        Attributes modAttrs = extractAttributeIds(m_allAttrIds);
        modifySchemaElementAttrs(modAttrs, mod_op, attrs);
        LDAPAttributeSchema modLdapAttribute = parseDefAttributes(modAttrs);
        m_schemaMgr.modifyAttribute(m_ldapAttribute, modLdapAttribute);
        m_ldapAttribute = modLdapAttribute;
    }

    public void modifyAttributes(String name, ModificationItem[] mods) throws NamingException {
        if (name.length() != 0) { // no subcontexts
            throw new NameNotFoundException(name); // no such object
        }
        Attributes modAttrs = extractAttributeIds(m_allAttrIds);
        modifySchemaElementAttrs(modAttrs, mods);
        LDAPAttributeSchema modLdapAttribute = parseDefAttributes(modAttrs);
        m_schemaMgr.modifyAttribute(m_ldapAttribute, modLdapAttribute);
        m_ldapAttribute = modLdapAttribute;
    }

    public void modifyAttributes(Name name, int mod_op, Attributes attrs) throws NamingException {
        modifyAttributes(name.toString(), mod_op, attrs);
    }

    public void modifyAttributes(Name name, ModificationItem[] mods) throws NamingException {
        modifyAttributes(name.toString(), mods);
    }
    
    /**
     * Search operations are not implemented because of complexity. Ir will require
     * to implement the full LDAP search filter sematics in the client. (The search 
     * filter sematics is implemented by the Directory server).
     */
    
}
