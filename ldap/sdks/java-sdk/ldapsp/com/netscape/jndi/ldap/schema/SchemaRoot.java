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

public class SchemaRoot extends SchemaDirContext {

    static final String m_className = "javax.naming.directory.DirContext"; // for class name is bindings
    
    SchemaDirContext m_classContainer, m_attrContainer, m_matchRuleContainer;
    
    SchemaManager m_schemaMgr;
    
    public SchemaRoot(LDAPConnection ld) throws NamingException{
        m_path = "";
        m_schemaMgr = new SchemaManager(ld);
        m_classContainer = new SchemaObjectClassContainer(m_schemaMgr);
        m_attrContainer = new SchemaAttributeContainer(m_schemaMgr);
        m_matchRuleContainer = new SchemaMatchingRuleContainer(m_schemaMgr);
    }

    SchemaObjectSubordinateNamePair resolveSchemaObject(String name) throws NamingException{
        
        SchemaDirContext obj = null;
        
        if (name.length() == 0) {
            obj = this;
        }
        else if (name.startsWith(CLASSDEF) || name.startsWith(CLASSDEF.toLowerCase())) {
            name = name.substring(CLASSDEF.length());
            obj = m_classContainer;
        }    
        else if (name.startsWith(ATTRDEF) || name.startsWith(ATTRDEF.toLowerCase())) {
            name = name.substring(ATTRDEF.length());
            obj = m_attrContainer;
        }    
        else if (name.startsWith(MRULEDEF) || name.startsWith(MRULEDEF.toLowerCase())) {
            name = name.substring(MRULEDEF.length());
            obj = m_matchRuleContainer;
            
        }
        else {
            throw new NameNotFoundException(name);
        }
        
        if (name.length() > 1 && name.startsWith("/")) {
            name = name.substring(1);
        }    
        return new SchemaObjectSubordinateNamePair(obj, name);
    }    
    

    /**
     * Attribute Operations
     */

    public Attributes getAttributes(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);        
        if (objNamePair.schemaObj == this) {
            throw new OperationNotSupportedException();
        }
        else {
            return objNamePair.schemaObj.getAttributes(objNamePair.subordinateName);
        }
    }

    public Attributes getAttributes(String name, String[] attrIds) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);        
        if (objNamePair.schemaObj == this) {
            throw new OperationNotSupportedException();
        }
        else {
            return objNamePair.schemaObj.getAttributes(objNamePair.subordinateName, attrIds);
        }
    }

    public Attributes getAttributes(Name name) throws NamingException {
        return getAttributes(name.toString());
    }

    public Attributes getAttributes(Name name, String[] attrIds) throws NamingException {
        return getAttributes(name.toString(), attrIds);
    }

    public void modifyAttributes(String name, int mod_op, Attributes attrs) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void modifyAttributes(String name, ModificationItem[] mods) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void modifyAttributes(Name name, int mod_op, Attributes attrs) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void modifyAttributes(Name name, ModificationItem[] mods) throws NamingException {
        throw new OperationNotSupportedException();
    }

    /**
     * Ldap entry operations
     */

    public Context createSubcontext(String name) throws NamingException {
        // Directory entry must have attributes
        throw new OperationNotSupportedException();
    }

    public Context createSubcontext(Name name) throws NamingException {
        // Directory entry must have attributes
        throw new OperationNotSupportedException();
    }

    public DirContext createSubcontext(String name, Attributes attrs) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            throw new OperationNotSupportedException();
        }
        else {
            return objNamePair.schemaObj.createSubcontext(objNamePair.subordinateName, attrs);
        }
    }

    public DirContext createSubcontext(Name name, Attributes attrs) throws NamingException {
        return createSubcontext(name.toString(), attrs);
    }

    public void destroySubcontext(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            throw new OperationNotSupportedException();
        }
        else {
            objNamePair.schemaObj.destroySubcontext(objNamePair.subordinateName);
        }
    }

    public void destroySubcontext(Name name) throws NamingException {
        destroySubcontext(name.toString());
    }

    /**
     * Naming Bind operations
     */

    public void bind(String name, Object obj) throws NamingException {
        if (obj instanceof DirContext) {
            createSubcontext(name, ((DirContext)obj).getAttributes(""));
        }
        else {
            throw new IllegalArgumentException("Can not bind this type of object");
        }    
    }

    public void bind(Name name, Object obj) throws NamingException {
        bind(name.toString(), obj);
    }

    public void rebind(String name, Object obj) throws NamingException {
        unbind(name);
        bind(name, obj);
    }

    public void rebind(Name name, Object obj) throws NamingException {
        rebind(name.toString(), obj);
    }

    public void rename(String oldName, String newName) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public void rename(Name oldName, Name newName) throws NamingException {
        rename(oldName.toString(), newName.toString());
    }

    public void unbind(String name) throws NamingException {
        // In ldap every entry is naming context
        destroySubcontext(name);
    }

    public void unbind(Name name) throws NamingException {
        // In ldap every entry is naming context
        destroySubcontext(name);
    }

    /**
     * List Operations
     */

    public NamingEnumeration list(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            return new SchemaRootNameClassPairEnum();
        }
        else {
            return objNamePair.schemaObj.list(objNamePair.subordinateName);
        }
    }

    public NamingEnumeration list(Name name) throws NamingException {
        return list(name.toString());
    }

    public NamingEnumeration listBindings(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            return new SchemaRootBindingEnum();
        }
        else {
            return objNamePair.schemaObj.listBindings(objNamePair.subordinateName);
        }

    }

    public NamingEnumeration listBindings(Name name) throws NamingException {
        return listBindings(name.toString());
    }

    /**
     * Lookup Operations
     */

    public Object lookup(String name) throws NamingException {
        SchemaObjectSubordinateNamePair objNamePair = resolveSchemaObject(name);
        if (objNamePair.schemaObj == this) {
            return this;
        }
        else {
            return objNamePair.schemaObj.lookup(objNamePair.subordinateName);
        }

    }

    public Object lookup(Name name) throws NamingException {
        return lookup(name.toString());
    }

    public Object lookupLink(String name) throws NamingException {
        throw new OperationNotSupportedException();
    }

    public Object lookupLink(Name name) throws NamingException {
        throw new OperationNotSupportedException();
    }

    /**
     * Test program
     */
    public static void main(String args[]) {
        try {
            String name = args[0];
            System.out.println((new SchemaRoot(null)).resolveSchemaObject(name));
        }
        catch (Exception e) {
            System.err.println(e);
        }    
    }
    
    /**
     * NamigEnumeration of NameClassPairs
     */
    class SchemaRootNameClassPairEnum implements NamingEnumeration {

        private int m_idx = -1;

        public Object next() {
            return nextElement();
        }

        public Object nextElement() {
            m_idx++;
            if (m_idx == 0 ) {
                return new NameClassPair(CLASSDEF, m_className);
            }
            else if (m_idx == 1) {
                return new NameClassPair(ATTRDEF, m_className);
            }
            else if (m_idx == 2) {
                return new NameClassPair(MRULEDEF, m_className);
            }
            else {
                throw new NoSuchElementException("SchemaRootEnumerator");
            }    
                
        }

        public boolean hasMore() {
            return hasMoreElements();
        }

        public boolean hasMoreElements() {
            return m_idx < 2;
        }

        public void close() {}
    }

    /**
     * NamingEnumeration of Bindings
     */
    class SchemaRootBindingEnum implements NamingEnumeration {

        private int m_idx = -1;

        public Object next() {
            return nextElement();
        }

        public Object nextElement() {
            m_idx++;
            if (m_idx == 0 ) {
                return new Binding(CLASSDEF, m_className, m_classContainer);
            }
            else if (m_idx == 1) {
                return new Binding(ATTRDEF, m_className, m_attrContainer);
            }
            else if (m_idx == 2) {
                return new Binding(MRULEDEF, m_className, m_matchRuleContainer);
            }
            else {
                throw new NoSuchElementException("SchemaRootEnumerator");
            }    
                
        }

        public boolean hasMore() {
            return hasMoreElements();
        }

        public boolean hasMoreElements() {
            return m_idx < 2;
        }

        public void close() {}
    }
}
