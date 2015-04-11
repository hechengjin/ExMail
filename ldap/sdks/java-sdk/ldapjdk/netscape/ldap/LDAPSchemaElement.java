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

import java.util.*;

/**
 *
 * Abstract class representing an element (such as an object class
 * definition, an attribute type definition, or a matching rule
 * definition) in the schema. The specific types of elements are
 * represented by the <CODE>LDAPObjectClassSchema</CODE>,
 * <CODE>LDAPAttributeSchema</CODE>, and <CODE>LDAPMatchingRuleSchema</CODE>
 * subclasses.
 * <P>
 *
 * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * that need to be specified in the definition of an object class,
 * attribute type, or matching rule.  All of these schema elements
 * can specify the following information:
 * <P>
 *
 * <UL>
 * <LI>a name identifying the element
 * <LI>an OID identifying the element
 * <LI>a description of the element
 * <LI>a qualifier "OBSOLETE"
 * </UL>
 * <P>
 *
 * In addition, there are optional standard qualifiers for attribute
 * types (see LDAPAttributeSchema), and implementation-specific
 * qualifiers may be added. Non-standard qualifiers must have names
 * starting with X-, e.g. "X-OWNER 'John Jacobson'". Optional and
 * non-standard qualifiers can be accessed with <CODE>getQualifier</CODE> and
 * <CODE>setQualifier</CODE>, and enumerated with
 * <CODE>getQualifierNames</CODE>.
 * <P>
 *
 * The <CODE>LDAPSchemaElement</CODE> class implements methods that
 * you can use with different types of schema elements (object class
 * definitions, attribute type definitions, and matching rule definitions).
 * You can do the following:
 * <UL>
 * <LI>get the name of a schema element
 * <LI>get the OID of a schema element
 * <LI>get the description of a schema element
 * <LI>add an element to the schema
 * <LI>remove an element from the schema
 * </UL>
 * <P>
 *
 * @see netscape.ldap.LDAPObjectClassSchema
 * @see netscape.ldap.LDAPAttributeSchema
 * @see netscape.ldap.LDAPMatchingRuleSchema
 * @version 1.0
 **/

public abstract class LDAPSchemaElement implements java.io.Serializable {

    static final long serialVersionUID = -3972153461950418863L;

    /**
     * Constructs a blank element.
     */
    protected LDAPSchemaElement() {
    }

    /**
     * Constructs a definition explicitly.
     * @param name name of element
     * @param oid dotted-string object identifier
     * @param description description of element
     */
    protected LDAPSchemaElement( String name, String oid,
                                 String description ) {
        this( name, oid, description, null );
    }

    /**
     * Constructs a definition explicitly.
     * @param name name of element
     * @param oid dotted-string object identifier
     * @param description description of element
     * @param aliases names which are to be considered aliases for this
     * element; <CODE>null</CODE> if there are no aliases
     */
    protected LDAPSchemaElement( String name, String oid,
                                 String description, String[] aliases ) {
        if ( oid == null ) {
            throw new IllegalArgumentException( "OID required" );
        }
        this.name = name;
        this.oid = oid;
        this.description = description;
        if ( (aliases != null) && (aliases.length > 0) ) {
            this.aliases = aliases;
        }
    }

    /**
     * Gets the name of the object class, attribute type, or matching rule.
     * @return the name of the object class, attribute type, or
     * matching rule.
     */
    public String getName() {
        return name;
    }

    /**
     * Gets the object ID (OID) of the object class, attribute type,
     * or matching rule in dotted-string format (for example, "1.2.3.4").
     * @return the OID of the object class, attribute type,
     * or matching rule.
     */
    public String getID() {
        return oid;
    }

    /**
     * Gets the object ID (OID) of the object class, attribute type,
     * or matching rule in dotted-string format (for example, "1.2.3.4").
     * @return the OID of the object class, attribute type,
     * or matching rule.
     * @deprecated Use <CODE>LDAPSchemaElement.getID()</CODE>
     */
    public String getOID() {
        return getID();
    }

    /**
     * Gets the description of the object class, attribute type,
     * or matching rule.
     * @return the description of the object class, attribute type,
     * or matching rule.
     */
    public String getDescription() {
        return description;
    }

    /**
     * Adds, removes or modifies the definition from a Directory.
     * @param ld an open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op type of modification to make
     * @param attr attribute in the schema entry to modify
     * @exception LDAPException if the definition can't be added/removed
     */
    protected void update( LDAPConnection ld, int op, LDAPAttribute attr,
                           String dn )
                           throws LDAPException {
        LDAPAttribute[] attrs = new LDAPAttribute[1];
        attrs[0] = attr;
        update( ld, op, attrs, dn );
    }

    /**
     * Adds, removes or modifies the definition from a Directory.
     * @param ld an open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op type of modification to make
     * @param attrs attributes in the schema entry to modify
     * @exception LDAPException if the definition can't be added/removed.
     */
    protected void update( LDAPConnection ld, int op, LDAPAttribute[] attrs,
                           String dn )
                           throws LDAPException {
        LDAPModificationSet mods = new LDAPModificationSet();
        for( int i = 0; i < attrs.length; i++ ) {
            mods.add( op, attrs[i] );
        }
        String entryName = LDAPSchema.getSchemaDN( ld, dn );
        ld.modify( entryName, mods );
    }

    /**
     * Adds, removes or modifies the definition from a Directory.
     * @param ld an open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op type of modification to make
     * @param name name of attribute in the schema entry to modify
     * @exception LDAPException if the definition can't be added/removed
     */
    protected void update( LDAPConnection ld, int op, String name,
                           String dn )
                           throws LDAPException {
        boolean quotingBug =
            !LDAPSchema.isAttributeSyntaxStandardsCompliant( ld );
        LDAPAttribute attr = new LDAPAttribute( name,
                                                getValue( quotingBug ) );
        update( ld, op, attr, dn );
    }

    /**
     * Adds the current object class, attribute type, or matching rule
     * definition to the schema. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld the <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server
     * @param dn the entry at which to add the schema definition
     * @exception LDAPException if the specified definition cannot be
     * added to the schema
     */
    public void add( LDAPConnection ld, String dn ) throws LDAPException {
        update( ld, LDAPModification.ADD, attrName, dn );
    }

    /**
     * Adds the current object class, attribute type, or matching rule
     * definition to the schema at the root DSE. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld the <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server
     * @exception LDAPException if the specified definition cannot be
     * added to the schema.
     */
    public void add( LDAPConnection ld ) throws LDAPException {
        add( ld, "" );
    }

    /**
     * Replaces a single value of the object class, attribute type,
     * or matching rule definition in the schema. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld the <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server
     * @param newValue the new value
     * @param dn the entry at which to modify the schema definition
     * @exception LDAPException if the specified definition cannot be
     * modified.
     */
    public void modify( LDAPConnection ld, LDAPSchemaElement newValue,
                        String dn )
                        throws LDAPException {
        boolean quotingBug =
            !LDAPSchema.isAttributeSyntaxStandardsCompliant( ld );
        LDAPModificationSet mods = new LDAPModificationSet();
        mods.add( LDAPModification.DELETE,
                  new LDAPAttribute( attrName, getValue( quotingBug ) ) );
        mods.add( LDAPModification.ADD,
                  new LDAPAttribute( attrName,
                                     newValue.getValue( quotingBug ) ) );
        String entryName = LDAPSchema.getSchemaDN( ld, dn );
        ld.modify( entryName, mods );
    }

    /**
     * Replaces a single value of the object class, attribute type,
     * or matching rule definition in the schema at the root DSE.
     * Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld the <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server
     * @param newValue the new value
     * @exception LDAPException if the specified definition cannot be
     * modified.
     */
    public void modify( LDAPConnection ld, LDAPSchemaElement newValue )
                        throws LDAPException {
        modify( ld, newValue, "" );
    }

    /**
     * Removes the current object class, attribute type, or matching rule
     * definition from the schema. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld the <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server
     * @param dn the entry at which to remove the schema definition
     * @exception LDAPException if the specified definition cannot be
     * removed from the schema.
     */
    public void remove( LDAPConnection ld, String dn ) throws LDAPException {
        update( ld, LDAPModification.DELETE, attrName, dn );
    }

    /**
     * Removes the current object class, attribute type, or matching rule
     * definition from the schema at the root DSE. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld the <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server
     * @exception LDAPException if the specified definition cannot be
     * removed from the schema
     */
    public void remove( LDAPConnection ld ) throws LDAPException {
        remove( ld, "" );
    }

    /**
     * Reports if the element is marked as obsolete.
     * @return <CODE>true<CODE> if the element is defined as obsolete.
     */
    public boolean isObsolete() {
        return (properties == null) ? false :
            properties.containsKey(OBSOLETE);
    }

    /**
     * Parses a raw schema value into OID, name, description, and
     * a Hashtable of other qualifiers and values.
     *
     * @param raw a raw schema definition
     */
    protected void parseValue( String raw ) {
        if ( properties == null ) {
            properties = new Hashtable();
        }
        int l = raw.length();
        // Processing is faster in char array than in String
        char[] ch = new char[l];
        raw.getChars( 0, l, ch, 0 );
        // Trim leading and trailing space
        l--;
        while( ch[l] == ' ' ) {
            l--;
        }
        int start = 0;
        while( ch[start] == ' ' ) {
            start++;
        }
        // Skip past "( " and ")" to start of OID
        start += 2;
        // Find end of OID
        int ind = start + 1;
        while( ch[ind] != ' ' ) {
            ind++;
        }
        oid = new String( ch, start, ind - start );

        ind = ind + 1;
        String s;
        String val;
        while ( ind < l ) {
            // Skip past blanks to start of next token
            while( ch[ind] == ' ' ) {
                ind++;
            }
            // Find end of token
            int last = ind + 1;
            while( (last < l) && (ch[last] != ' ') )
                last++;
            if ( last < l ) {
                // Found a token
                s = new String( ch, ind, last-ind );
                ind = last;
                if ( novalsTable.containsKey( s ) ) {
                    properties.put( s, "" );
                    continue;
                }
            } else {
                // Reached end of string with no end of token
                s = "";
                ind = l;
                break;
            }

            // Find the start of the value of the token
            while( (ind < l) && (ch[ind] == ' ') ) {
                ind++;
            }
            last = ind + 1;
            if ( ind >= l ) {
                break;
            }

            boolean quoted = false;
            boolean list = false;
            if ( ch[ind] == '\'' ) {
                // The value is quoted
                quoted = true;
                ind++;
                while( (last < l) && (ch[last] != '\'') ) {
                    last++;
                }
            } else if ( ch[ind] == '(' ) {
                // The value is a list
                list = true;
                ind++;
                while( (last < l) && (ch[last] != ')') ) {
                    last++;
                }
            } else {
                // The value is not quoted
                while( (last < l) && (ch[last] != ' ') ) {
                    last++;
                }
            }
            if ( (ind < last) && (last <= l) ) {
                if ( list ) {
                    Vector v = new Vector();
                    if ( ch[ind] == ' ' ) {
                        ind++;
                    }
                    val = new String( ch, ind, last-ind-1 );
                    // Is this a quoted list? If so, use ' as delimiter,
                    // otherwise use ' '. The space between quoted
                    // values will be returned as tokens containing only
                    // white space. White space is not valid in a list
                    // value, so we just remove all tokens containing
                    // only white space.
                    String delim = (val.indexOf( '\'' ) >= 0) ? "'" : " ";
                    StringTokenizer st = new StringTokenizer( val, delim );
                    while ( st.hasMoreTokens() ) {
                        String tok = st.nextToken().trim();
                        if ( (tok.length() > 0) && !tok.equals( "$" ) ) {
                            v.addElement( tok );
                        }
                    }
                    properties.put( s, v );
                } else {
                    val = new String( ch, ind, last-ind );
                    if ( s.equals( "NAME" ) ) {
                        name = val;
                    } else if ( s.equals( "DESC" ) ) {
                        description = val;
                    } else {
                        properties.put( s, val );
                    }
                    if ( quoted ) {
                        last++;
                    }
                }
            }
            ind = last + 1;
        }
        // Aliases end up as values of NAME
        String[] vals = getQualifier( "NAME" );
        if ( (vals != null) && (vals.length > 0) ) {
            name = vals[0];
            if ( vals.length > 1 ) {
                aliases = new String[vals.length-1];
                System.arraycopy( vals, 1, aliases, 0, aliases.length );
            }
        }
    }

    /**
     * Formats a String in the format defined in X.501 (see
     * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * >RFC 2252, Lightweight Directory Access Protocol
     * (v3): Attribute Syntax Definitions</A>
     * for a description of this format).
     * This is the format that LDAP servers and clients use to exchange
     * schema information. For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of the "attributetypes" attribute are attribute type
     * descriptions in this format.
     * <P>
     * @return a formatted String for defining a schema element.
     */
    public String getValue() {
        return getValue( false );
    }

    String getValue( boolean quotingBug ) {
        return null;
    }

    /**
     * Prepares the initial common part of a schema element value in
     * RFC 2252 format for submitting to a server
     *
     * @return the OID, name, description, and possibly OBSOLETE
     * fields of a schema element definition.
     */
    String getValuePrefix() {
        String s = "( " + oid + ' ';
        if ( (name != null) && (name.length() > 0) ) {
            s += "NAME ";
            if ( aliases != null ) {
                s += "( " + '\'' + name + "\' ";
                for( int i = 0; i < aliases.length; i++ ) {
                    s += '\'' + aliases[i] + "\' ";
                }
                s += ") ";
            } else {
                s += '\'' + name + "\' ";
            }
        }
        if ( description != null ) {
            s += "DESC \'" + description + "\' ";
        }
        if ( isObsolete() ) {
            s += OBSOLETE + ' ';
        }
        return s;
    }

    /**
     * Gets qualifiers which may or may not be present
     *

     * @param names list of qualifiers to look up
     * @return String in RFC 2252 format containing any values
     * found, not terminated with ' '.
     */
    protected String getOptionalValues( String[] names ) {
        String s = "";
        for( int i = 0; i < names.length; i++ ) {
            String[] vals = getQualifier( names[i] );
            if ( (vals != null) && (vals.length > 0) ) {
                s += names[i] + ' ' + vals[0];
            }
        }
        return s;
    }

    /**
     * Gets any qualifiers marked as custom (starting with "X-")
     *
     * @return string in RFC 2252 format, without a terminating
     * ' '.
     */
    protected String getCustomValues() {
        String s = "";
        Enumeration en = properties.keys();
        while( en.hasMoreElements() ) {
            String key = (String)en.nextElement();
            if ( !key.startsWith( "X-" ) ) {
                continue;
            }
            s += getValue( key, true, false ) + ' ';
        }
        // Strip trailing ' '
        if ( (s.length() > 0) && (s.charAt( s.length() - 1 ) == ' ') ) {
            s = s.substring( 0, s.length() - 1 );
        }
        return s;
    }

    /**
     * Gets a qualifier's value or values, if present, and formats
     * the String according to RFC 2252
     *
     * @param key the qualifier to get
     * @param doQuote <CODE>true</CODE> if values should be enveloped
     * with single quotes
     * @param doDollar <CODE>true</CODE> if a list of values should use
     * " $ " as separator; that is true for object class attribute lists
     * @return String in RFC 2252 format, without a terminating
     * ' '.
     */
    String getValue( String key, boolean doQuote, boolean doDollar ) {
        String s = "";
        Object o = properties.get( key );
        if ( o == null ) {
            return s;
        }
        if ( o instanceof String ) {
            if ( ((String)o).length() > 0 ) {
                s += key + ' ';
                if ( doQuote ) {
                    s += '\'';
                }
                s += (String)o;
                if ( doQuote ) {
                    s += '\'';
                }
            }
        } else {
            s += key + " ( ";
            Vector v = (Vector)o;
            for( int i = 0; i < v.size(); i++ ) {
                if ( doQuote ) {
                    s += '\'';
                }
                s += (String)v.elementAt(i);
                if ( doQuote ) {
                    s += '\'';
                }
                s += ' ';
                if ( doDollar && (i < (v.size() - 1)) ) {
                    s += "$ ";
                }
            }
            s += ')';
        }

        return s;
    }

    /**
     * Gets a qualifier's value or values, if present, and format
     * the String according to RFC 2252.
     *
     * @param key the qualifier to get
     * @param doQuote <CODE>true</CODE> if values should be enveloped
     * with single quotes
     * @return String in RFC 2252 format, without a terminating
     * ' '.
     */
    String getValue( String key, boolean doQuote ) {
        return getValue( key, doQuote, true );
    }

    /**
     * Keeps track of qualifiers which are not predefined.
     * @param name name of qualifier
     * @param value value of qualifier. "" for no value, <CODE>null</CODE>
     * to remove the qualifier
     */
    public void setQualifier( String name, String value ) {
        if ( properties == null ) {
            properties = new Hashtable();
        }
        if ( value != null ) {
            properties.put( name, value );
        } else {
            properties.remove( name );
        }
    }

    /**
     * Keeps track of qualifiers which are not predefined.
     * @param name name of qualifier
     * @param values array of values
     */
    public void setQualifier( String name, String[] values ) {
        if ( values == null ) {
            return;
        }
        if ( properties == null ) {
            properties = new Hashtable();
        }
        Vector v = new Vector();
        for( int i = 0; i < values.length; i++ ) {
            v.addElement( values[i] );
        }
        properties.put( name, v );
    }

    /**
     * Gets the value of a qualifier which is not predefined.
     * @param name name of qualifier
     * @return value or values of qualifier; <CODE>null</CODE> if not
     * present, a zero-length array if present but with no value.
     */
    public String[] getQualifier( String name ) {
        if ( properties == null ) {
            return null;
        }
        Object o = properties.get( name );
        if ( o == null ) {
            return null;
        }
        if ( o instanceof Vector ) {
            Vector v = (Vector)o;
            String[] vals = new String[v.size()];
            v.copyInto( vals );
            return vals;
        }
        String s = (String)o;
        if ( s.length() < 1 ) {
            return new String[0];
        } else {
            return new String[] { s };
        }
    }

    /**
     * Gets an enumeration of all qualifiers which are not predefined.
     * @return enumeration of qualifiers.
     */
    public Enumeration getQualifierNames() {
        return properties.keys();
    }

    /**
     * Gets the aliases of the attribute, if any
     * @return the aliases of the attribute, or <CODE>null</CODE> if
     * it does not have any aliases
     */
    public String[] getAliases() {
        return aliases;
    }

    /**
     * Creates a string for use in toString with any qualifiers of the element.
     *
     * @param ignore any qualifiers to NOT include
     * @return a String with any known qualifiers.
     */
    String getQualifierString( String[] ignore ) {
        Hashtable toIgnore = null;
        if ( ignore != null ) {
            toIgnore = new Hashtable();
            for( int i = 0; i < ignore.length; i++ ) {
                toIgnore.put( ignore[i], ignore[i] );
            }
        }
        String s = "";
        Enumeration en = getQualifierNames();
        while( en.hasMoreElements() ) {
            String qualifier = (String)en.nextElement();
            if ( (toIgnore != null) && toIgnore.containsKey( qualifier ) ) {
                continue;
            }
            s += "; " + qualifier;
            String[] vals = getQualifier( qualifier );
            if ( vals == null ) {
                s += ' ';
                continue;
            }
            s += ": ";
            for( int i = 0; i < vals.length; i++ ) {
                s += vals[i] + ' ';
            }
        }
        // Strip trailing ' '
        if ( (s.length() > 0) && (s.charAt( s.length() - 1 ) == ' ') ) {
            s = s.substring( 0, s.length() - 1 );
        }
        return s;
    }

    /**
     * Gets any aliases for this element
     *
     * @return a string with any aliases, for use in toString()
     */
    String getAliasString() {
        if ( aliases != null ) {
            String s = "; aliases:";
            for( int i = 0; i < aliases.length; i++ ) {
                s += ' ' + aliases[i];
            }
            return s;
        }
        return "";
    }

    // Constants for known syntax types
    public static final int unknown = 0;
    public static final int cis = 1;
    public static final int binary = 2;
    public static final int telephone = 3;
    public static final int ces = 4;
    public static final int dn = 5;
    public static final int integer = 6;

    protected static final String cisString =
                                      "1.3.6.1.4.1.1466.115.121.1.15";
    protected static final String binaryString =
                                      "1.3.6.1.4.1.1466.115.121.1.5";
    protected static final String telephoneString =
                                      "1.3.6.1.4.1.1466.115.121.1.50";
    protected static final String cesString =
                                      "1.3.6.1.4.1.1466.115.121.1.26";
    protected static final String intString =
                                      "1.3.6.1.4.1.1466.115.121.1.27";
    protected static final String dnString =
                                      "1.3.6.1.4.1.1466.115.121.1.12";
    // Predefined qualifiers which apply to any schema element type
    public static final String OBSOLETE = "OBSOLETE";
    public static final String SUPERIOR = "SUP";

    // Predefined qualifiers
    public static final String SYNTAX = "SYNTAX";

    // Properties which are common to all schema elements
    protected String oid = null;
    protected String name = "";
    protected String description = "";
    protected String attrName = null;
    protected String rawValue = null;
    protected String[] aliases = null;
    // Additional qualifiers
    protected Hashtable properties = null;
    // Qualifiers known to not have values
    static protected Hashtable novalsTable = new Hashtable();
}
