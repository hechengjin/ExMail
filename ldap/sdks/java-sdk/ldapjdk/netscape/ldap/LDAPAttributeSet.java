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
import netscape.ldap.client.*;
import netscape.ldap.client.opers.*;

/**
 * Represents a set of attributes (for example, the set of attributes
 * in an entry).
 *
 * @version 1.0
 * @see netscape.ldap.LDAPAttribute
 */
public class LDAPAttributeSet implements Cloneable, java.io.Serializable {
    static final long serialVersionUID = 5018474561697778100L;
    Hashtable attrHash = null;
    LDAPAttribute[] attrs = new LDAPAttribute[0];
    /* If there are less attributes than this in the set, it's not worth
     creating a Hashtable - faster and cheaper most likely to string
     comparisons. Most applications fetch attributes once only, anyway */
    static final int ATTR_COUNT_REQUIRES_HASH = 5;

    /**
     * Constructs a new set of attributes.  This set is initially empty.
     */
    public LDAPAttributeSet() {
    }

    /**
     * Constructs an attribute set.
     * @param attrs the list of attributes
     */
    public LDAPAttributeSet( LDAPAttribute[] attrs ) {
        this.attrs = attrs;
    }

    public synchronized Object clone() {
        try {
            LDAPAttributeSet attributeSet = new LDAPAttributeSet();
            attributeSet.attrs = new LDAPAttribute[attrs.length];
            for (int i = 0; i < attrs.length; i++) {
                attributeSet.attrs[i] = new LDAPAttribute(attrs[i]);
            }
             return attributeSet;
        } catch (Exception e) {
            return null;
        }
    }

    /**
     * Returns an enumeration of the attributes in this attribute set.
     * @return enumeration of the attributes in this set.
     */
    public Enumeration getAttributes () {
        Vector v = new Vector();
        synchronized(this) {
            for (int i=0; i<attrs.length; i++) {
                v.addElement(attrs[i]);
            }
        }
        return v.elements();
    }

    /**
     * Creates a new attribute set containing only the attributes
     * that have the specified subtypes.
     * <P>
     *
     * For example, suppose an attribute set contains the following attributes:
     * <P>
     *
     * <PRE>
     * cn
     * cn;lang-ja
     * sn;phonetic;lang-ja
     * sn;lang-us
     * </PRE>
     *
     * If you call the <CODE>getSubset</CODE> method and pass
     * <CODE>lang-ja</CODE> as the argument, the method returns
     * an attribute set containing the following attributes:
     * <P>
     *
     * <PRE>
     * cn;lang-ja
     * sn;phonetic;lang-ja
     * </PRE>
     *
     * @param subtype semi-colon delimited list of subtypes
     * to find within attribute names. 
     * For example:
     * <PRE>
     *     "lang-ja"        // Only Japanese language subtypes
     *     "binary"         // Only binary subtypes
     *     "binary;lang-ja" // Only Japanese language subtypes
     *                         which also are binary
     * </PRE>
     * @return attribute set containing the attributes that have
     * the specified subtypes.
     * @see netscape.ldap.LDAPAttribute
     * @see netscape.ldap.LDAPAttributeSet#getAttribute
     * @see netscape.ldap.LDAPEntry#getAttributeSet
     */
    public LDAPAttributeSet getSubset(String subtype) {
        LDAPAttributeSet attrs = new LDAPAttributeSet();
        if ( subtype == null )
            return attrs;
        StringTokenizer st = new StringTokenizer(subtype, ";");
        if( st.countTokens() < 1 )
            return attrs;
        String[] searchTypes = new String[st.countTokens()];
        int i = 0;
        while( st.hasMoreElements() ) {
            searchTypes[i] = (String)st.nextElement();
            i++;
        }
        Enumeration attrEnum = getAttributes();
        while( attrEnum.hasMoreElements() ) {
            LDAPAttribute attr = (LDAPAttribute)attrEnum.nextElement();
            if( attr.hasSubtypes( searchTypes ) )
                attrs.add( new LDAPAttribute( attr ) );
        }
        return attrs;
    }

    /**
     * Returns a single attribute that exactly matches the specified attribute
     * name.
     * @param attrName name of attribute to return
     * For example:
     *<PRE>
     *     "cn"            // Only a non-subtyped version of cn
     *     "cn;lang-ja"    // Only a Japanese version of cn
     *</PRE>
     * @return attribute that has exactly the same name, or null
     * (if no attribute in the set matches the specified name).
     * @see netscape.ldap.LDAPAttribute
     */
    public LDAPAttribute getAttribute( String attrName ) {
        prepareHashtable();
        if (attrHash != null) {
            return (LDAPAttribute)attrHash.get( attrName.toLowerCase() );
        } else {
            for (int i = 0; i < attrs.length; i++) {
                if (attrName.equalsIgnoreCase(attrs[i].getName())) {
                    return attrs[i];
                }
            }
            return null;
        }
    }

    /**
     * Prepares hashtable for fast attribute lookups.
     */
    private void prepareHashtable() {
        if ((attrHash == null) && (attrs.length >= ATTR_COUNT_REQUIRES_HASH)) {
            attrHash = new Hashtable();
            for (int j = 0; j < attrs.length; j++) {
                attrHash.put( attrs[j].getName().toLowerCase(), attrs[j] );
            }
        }
    }

    /**
     * Returns the subtype that matches the attribute name specified
     * by <CODE>attrName</CODE> and the language specificaton identified
     * by <CODE>lang</CODE>.
     * <P>
     *
     * If no attribute in the set has the specified name and subtype,
     * the method returns <CODE>null</CODE>.
     *
     * Attributes containing subtypes other than <CODE>lang</CODE>
     * (for example, <CODE>cn;binary</CODE>) are returned only if
     * they contain the specified <CODE>lang</CODE> subtype and if
     * the set contains no attribute having only the <CODE>lang</CODE>
     * subtype.  (For example, <CODE>getAttribute( "cn", "lang-ja" )</CODE>
     * returns <CODE>cn;lang-ja;phonetic</CODE> only if the 
     * <CODE>cn;lang-ja</CODE> attribute does not exist.)
     * <P>
     *
     * If null is specified for the <CODE>lang</CODE> argument,
     * calling this method is the same as calling the
     * <CODE>getAttribute(attrName)</CODE> method.
     * <P>
     *
     * For example, suppose an entry contains only the following attributes:
     * <P>
     * <UL>
     * <LI><CODE>cn;lang-en</CODE>
     * <LI><CODE>cn;lang-ja-JP-kanji</CODE>
     * <LI><CODE>sn</CODE>
     * </UL>
     * <P>
     *
     * Calling the following methods will return the following values:
     * <P>
     * <UL>
     * <LI><CODE>getAttribute( "cn" )</CODE> returns <CODE>null</CODE>.
     * <LI><CODE>getAttribute( "sn" )</CODE> returns the "<CODE>sn</CODE>" attribute.
     * <LI><CODE>getAttribute( "cn", "lang-en-us" )</CODE> returns the "<CODE>cn;lang-en</CODE>" attribute.
     * <LI><CODE>getAttribute( "cn", "lang-en" )</CODE> returns the "<CODE>cn;lang-en</CODE>" attribute.
     * <LI><CODE>getAttribute( "cn", "lang-ja" )</CODE> returns <CODE>null</CODE>.
     * <LI><CODE>getAttribute( "sn", "lang-en" )</CODE> returns the "<CODE>sn</CODE>" attribute.
     *</UL>
     * <P>
     * @param attrName name of attribute to find in the entry
     * @param lang a language specification
     * @return the attribute that matches the base name and that best
     * matches any specified language subtype.
     * @see netscape.ldap.LDAPAttribute
     */
    public LDAPAttribute getAttribute( String attrName, String lang ) {
        if ( (lang == null) || (lang.length() < 1) )
            return getAttribute( attrName );

        String langLower = lang.toLowerCase();
        if ((langLower.length() < 5) ||
            ( !langLower.substring( 0, 5 ).equals( "lang-" ) )) {
            return null;
        }
        StringTokenizer st = new StringTokenizer( langLower, "-" );
        // Skip first token, which is "lang-"
        st.nextToken();
        String[] langComponents = new String[st.countTokens()];
        int i = 0;
        while ( st.hasMoreTokens() ) {
            langComponents[i] = st.nextToken();
            i++;
        }

        String searchBasename = LDAPAttribute.getBaseName(attrName);
        String[] searchTypes = LDAPAttribute.getSubtypes(attrName);
        LDAPAttribute found = null;
        int matchCount = 0;
        for( i = 0; i < attrs.length; i++ ) {
            boolean isCandidate = false;
            LDAPAttribute attr = attrs[i];
            // Same base name?
            if ( attr.getBaseName().equalsIgnoreCase(searchBasename) ) {
                // Accept any subtypes?
                if( (searchTypes == null) || (searchTypes.length < 1) ) {
                    isCandidate = true;
                } else {
                    // No, have to check each subtype for inclusion
                    if( attr.hasSubtypes( searchTypes ) )
                    isCandidate = true;
                }
            }
            String attrLang = null;
            if ( isCandidate ) {
                attrLang = attr.getLangSubtype();
                // At this point, the base name and subtypes are okay
                if ( attrLang == null ) {
                    // If there are no language attributes, this one is okay
                    found = attr;
                } else {
                    // We just have to check for language match
                    st = new StringTokenizer( attrLang.toLowerCase(), "-" );
                    // Skip first token, which is "lang-"
                    st.nextToken();
                    // No match if the attribute's language spec is longer
                    // than the target one
                    if ( st.countTokens() > langComponents.length )
                        continue;

                    // How many subcomponents of the language match?
                    int j = 0;
                    while ( st.hasMoreTokens() ) {
                        if ( !langComponents[j].equals( st.nextToken() ) ) {
                            j = 0;
                            break;
                        }
                        j++;
                    }
                    if ( j > matchCount ) {
                        found = attr;
                        matchCount = j;
                    }
                }
            }
        }
        return found;
    }

    /**
     * Returns the attribute at the position specified by the index.
     * For example, if you specify the index 0, the method returns the
     * first attribute in the set. The index is 0-based.
     *
     * @param index index of the attribute to obtain
     * @return attribute at the position specified by the index.
     */
    public LDAPAttribute elementAt (int index) {
        return attrs[index];
    }

    /**
     * Removes the attribute at the position specified by the index.
     * For example, if you specify the index 0, the method removes the
     * first attribute in the set.  The index is 0-based.
     *
     * @param index index of the attribute to remove
     */
    public void removeElementAt (int index) {
        if ((index >= 0) && (index < attrs.length)) {
            synchronized(this) {
                LDAPAttribute[] vals = new LDAPAttribute[attrs.length-1];
                int j = 0;
                for (int i = 0; i < attrs.length; i++) {
                    if (i != index) {
                        vals[j++] = attrs[i];
                    }
                }
                if (attrHash != null) {
                    attrHash.remove(attrs[index].getName().toLowerCase());
                }
                attrs = vals;
            }
        }
    }

    /**
     * Returns the number of attributes in this set.
     * @return number of attributes in this attribute set.
     */
    public int size () {
        return attrs.length;
    }

    /**
     * Adds the specified attribute to this attribute set.
     * @param attr attribute to add to this set
     */
    public synchronized void add( LDAPAttribute attr ) {
        if (attr != null) {
            LDAPAttribute[] vals = new LDAPAttribute[attrs.length+1];
            for (int i = 0; i < attrs.length; i++)
                vals[i] = attrs[i];
            vals[attrs.length] = attr;
            attrs = vals;
            if (attrHash != null) {
                attrHash.put( attr.getName().toLowerCase(), attr );
            }
        }
    }

    /**
     * Removes the specified attribute from the set.
     * @param name name of the attribute to remove
     */
    public synchronized void remove( String name ) {
        for( int i = 0; i < attrs.length; i++ ) {
            if ( name.equalsIgnoreCase( attrs[i].getName() ) ) {
                removeElementAt(i);
                break;
            }
        }
    }

    /**
     * Retrieves the string representation of all attributes
     * in the attribute set.  For example:
     *
     * <PRE>
     * LDAPAttributeSet: LDAPAttribute {type='cn', values='Barbara Jensen,Babs
     * Jensen'}LDAPAttribute {type='sn', values='Jensen'}LDAPAttribute {type='
     * givenname', values='Barbara'}LDAPAttribute {type='objectclass', values=
     * 'top,person,organizationalPerson,inetOrgPerson'}LDAPAttribute {type='ou',
     * values='Product Development,People'}
     * </PRE>
     *
     * @return string representation of all attributes in the set.
     */
    public String toString() {
        StringBuffer sb = new StringBuffer("LDAPAttributeSet: ");
        for( int i = 0; i < attrs.length; i++ ) {
            if (i != 0) {
                sb.append(" ");
            }            
            sb.append(attrs[i].toString());
        }
        return sb.toString();
    }
}
