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
package com.netscape.jndi.ldap;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;

import netscape.ldap.*;
import netscape.ldap.controls.*;

import com.netscape.jndi.ldap.common.*;

import java.util.*;

/**
 * Context Environment
 */
class ContextEnv extends ShareableEnv  {

    public static final String DEFAULT_HOST = "localhost";
    public static final int    DEFAULT_PORT = LDAPv2.DEFAULT_PORT;
    public static final int    DEFAULT_SSL_PORT = 636;
    public static final int    DEFAULT_LDAP_VERSION = 3;    

    // JNDI defined environment propertiies
    public static final String P_PROVIDER_URL = Context.PROVIDER_URL;
    public static final String P_SECURITY_PROTOCOL = Context.SECURITY_PROTOCOL;
    public static final String P_SECURITY_AUTHMODE = Context.SECURITY_AUTHENTICATION;
    public static final String P_USER_DN = Context.SECURITY_PRINCIPAL;
    public static final String P_USER_PASSWORD = Context.SECURITY_CREDENTIALS;
    public static final String P_APPLET = Context.APPLET;
    public static final String P_AUTHORITATIVE = Context.AUTHORITATIVE;
    public static final String P_LANGUAGE = Context.LANGUAGE;
    public static final String P_BATCHSIZE = Context.BATCHSIZE;
    public static final String P_REFERRAL_MODE = Context.REFERRAL;

    // Custom properties
    public static final String P_CONNECT_CTRLS = "java.naming.ldap.control.connect";
    public static final String P_BINARY_ATTRS = "java.naming.ldap.attributes.binary";
    public static final String P_ATTRS_ONLY = "java.naming.ldap.typesOnly";
    public static final String P_DELETE_OLDRDN = "java.naming.ldap.deleteRDN";    
    public static final String P_SOCKET_FACTORY = "java.naming.ldap.factory.socket";
    public static final String P_CIPHER_SUITE = "java.naming.ldap.ssl.ciphers"; // new
    public static final String P_TIME_LIMIT = "java.naming.ldap.timelimit";     // new
    public static final String P_MAX_RESULTS = "java.naming.ldap.maxresults";   // new
    public static final String P_DEREF_ALIASES = "java.naming.ldap.derefAliases";
    public static final String P_REFERRAL_HOPLIMIT = "java.naming.referral.limit";
    public static final String P_LDAP_VERSION = "java.naming.ldap.version";
    public static final String P_JNDIREF_SEPARATOR = "java.naming.ref.separator";
    public static final String P_SASL_AUTHID = "java.naming.security.sasl.authorizationId";
    public static final String P_SASL_CALLBACK = "java.naming.security.sasl.callback";
    public static final String P_SASL_PKGS = "javax.security.sasl.client.pkgs";
    
    private static final String SASL_PROP_PREFIX = "javax.security.sasl";
    
    public static final String P_TRACE = LDAPConnection.TRACE_PROPERTY;
    
    // Possible values for the Context.REFERRAL env property
    private static final String V_REFERRAL_FOLLOW = "follow";
    private static final String V_REFERRAL_IGNORE = "ignore";
    private static final String V_REFERRAL_THROW_EXCEPTION = "throw";

    // Possible values for the java.naming.ldap.derefAliases env property
    private static final String V_DEREF_NEVER = "never";
    
    private static final String V_DEREF_SEARCHING = "searching";
    private static final String V_DEREF_FINDING = "finding";
    private static final String V_DEREF_ALWAYS = "always";
    
    // Possible values for the java.naming... env property
    private static final String V_AUTH_NONE = "none";
    private static final String V_AUTH_SIMPLE = "simple";

    /**
     * Constructor for non root Contexts
     *
     * @param parent A reference to the parent context environment
     * @param parentSharedEnvIdx index into parent's shared environemnt list
     */
    public ContextEnv(ShareableEnv parent, int parentSharedEnvIdx) {
        super(parent, parentSharedEnvIdx);
    }
    
    /**
     * Constructor for the root context
     * 
     * @param initialEnv a hashtable with environemnt properties
     */
    public ContextEnv(Hashtable initialEnv) {
        super(initialEnv);
    }

    /**
     * Clone ContextEnv
     *
     * @return A "clone" of the current context environment
     */
    /**
     * Clone ShareableEnv
     * The code is the same as in the superclass (ShareableEnv) except that  
     * a ContextEnv instance is returned
     *
     * @return A "clone" of the current context environment
     */
    public Object clone() {
        
        // First freeze updates for this context
        freezeUpdates();
        
         // If the context has been modified, then it is the parent of the clone
        if (m_sharedEnv != null) {
            return new ContextEnv(this, m_sharedEnv.size()-1);
        }
        
        // No changes has been done to the inherited parent context. Pass the parent
        // context to the clone
        else {
            return new ContextEnv(m_parentEnv, m_parentSharedEnvIdx);
        }    
    }    

    /**
     * Update property value. Properties that pertain to LDAPSearchConstraints
     * are immediately propagated. To take effect of properties that are connection
     * related, (like user name/password, ssl mode) the context mujst be reconnected
     * after the change of environment
     */
    Object updateProperty(String name, Object val, LDAPSearchConstraints cons) throws NamingException{
        Object oldVal = getProperty(name);
        setProperty(name,val);
        try {
            if (name.equalsIgnoreCase(P_BATCHSIZE)) {
                updateBatchSize(cons);
            }
            else if (name.equalsIgnoreCase(P_TIME_LIMIT)) {
                updateTimeLimit(cons);
            }
            else if (name.equalsIgnoreCase(P_MAX_RESULTS)) {
                updateMaxResults(cons);
            }
            else if (name.equalsIgnoreCase(P_DEREF_ALIASES)) {
                updateDerefAliases(cons);
            }
            else if (name.equalsIgnoreCase(P_REFERRAL_MODE)) {
                updateReferralMode(cons);
            }
            else if (name.equalsIgnoreCase(P_REFERRAL_HOPLIMIT)) {
                updateReferralHopLimit(cons);
            }            
        }
        catch (IllegalArgumentException e) {
            if (oldVal == null) {
                removeProperty(name);
            }
            else {
                setProperty(name, oldVal);
            }
            throw e;
        }
        return oldVal;
    }
    
    /**
     * Initialize LDAPSearchConstraints with environment properties
     */
    void updateSearchCons(LDAPSearchConstraints cons) throws NamingException{
        updateBatchSize(cons);
        updateTimeLimit(cons);
        updateMaxResults(cons);
        updateDerefAliases(cons);
        updateReferralMode(cons);
        updateReferralHopLimit(cons);
    }
    
    /**
     * Set the suggested number of result to return at a time during search in the
     * default SearchConstraints for the connection.
     * Specified with the env property java.naming.batchsize
     */
    void updateBatchSize(LDAPSearchConstraints cons) {
        String size = (String)getProperty(P_BATCHSIZE);
        if (size != null) {
            int n = -1;
            try {
                n = Integer.parseInt(size);
            }
            catch (Exception e) {
                throw new IllegalArgumentException("Illegal value for " + P_BATCHSIZE);
            }
            cons.setBatchSize(n);
        }
    }

    /**
     * Set the maximum number of milliseconds to wait for any operation under default
     * SearchConstraints for the connection.
     * Specified with the env property java.naming.ldap.timelimit
     * Note: sun ldap does not have this property
     */
    void updateTimeLimit(LDAPSearchConstraints cons) {
        String millis = (String)getProperty(P_TIME_LIMIT);
        if (millis != null) {
            int n = -1;
            try {
                n = Integer.parseInt(millis);
            }
            catch (Exception e) {
                throw new IllegalArgumentException("Illegal value for " + P_TIME_LIMIT);
            }
            cons.setTimeLimit(n);
        }
    }

    /**
     * Set the maximum number of search results to be returned under default
     * SearchConstraints for the connection.
     * Specified with the env property java.naming.ldap.maxresults
     * Note: sun ldap does not have this property
     */
    void updateMaxResults(LDAPSearchConstraints cons) {
        String max = (String)getProperty(P_MAX_RESULTS);
        if (max != null) {
            int n = -1;
            try {
                n = Integer.parseInt(max);
            }
            catch (Exception e) {
                throw new IllegalArgumentException(
                "Illegal value for " + P_MAX_RESULTS);
            }
            cons.setMaxResults(n);
        }
    }

    /**
     * Set how aliases should be dereferenced under default SearchConstraints for the
     * connection.
     * Specified with the env property java.naming.ldap.derefAliases
     */
    final void updateDerefAliases(LDAPSearchConstraints cons) throws IllegalArgumentException{
        String deref = (String)getProperty(P_DEREF_ALIASES);
        if(deref != null) {
            if(deref.equalsIgnoreCase(V_DEREF_NEVER)) {
                cons.setDereference(LDAPv2.DEREF_NEVER);
            }
            else if(deref.equalsIgnoreCase(V_DEREF_SEARCHING)) {
                        cons.setDereference(LDAPv2.DEREF_SEARCHING);
                    }
            else if(deref.equalsIgnoreCase(V_DEREF_FINDING)) {
                cons.setDereference(LDAPv2.DEREF_FINDING);
            }
            else if(deref.equalsIgnoreCase(V_DEREF_ALWAYS)) {
                cons.setDereference(LDAPv2.DEREF_ALWAYS);
            }
            else {
                throw new IllegalArgumentException("Illegal value for " + P_DEREF_ALIASES);
            }
        }
    }

    /**
     * Set referral parameters for the default SearchConstraints for the connection.
     * Specified with the env property java.naming.referral
     */
    void updateReferralMode(LDAPSearchConstraints cons) {
        String mode = (String)getProperty(P_REFERRAL_MODE);
        if(mode != null) {
            if(mode.equalsIgnoreCase(V_REFERRAL_FOLLOW)) {
                cons.setReferrals(true);
                String user = getUserDN(), passwd = getUserPassword();
                if (user != null && passwd != null) {
                    cons.setRebindProc(new ReferralRebindProc(user, passwd));
                }    
            }
            else if(mode.equalsIgnoreCase(V_REFERRAL_THROW_EXCEPTION)) {
                cons.setReferrals(false);
            }
            else if(mode.equalsIgnoreCase(V_REFERRAL_IGNORE)) {
                //If MANAGEDSAIT control is not supported by the server
                //(e.g. Active Directory) should enable exception and ignore it
                cons.setServerControls(new LDAPControl(
                    LDAPControl.MANAGEDSAIT, /*critical=*/false, null));
                cons.setReferrals(false);
            }
            else {
                throw new IllegalArgumentException("Illegal value for " + P_REFERRAL_MODE);
            }    
        }
    }

    /**
     * Check if referrals are to be ignored
     */
    boolean ignoreReferralsMode() {
        String mode = (String)getProperty(P_REFERRAL_MODE);
        return mode == null || mode.equalsIgnoreCase(V_REFERRAL_IGNORE);
    }

    /**
     * Implementation for LDAPRebind interface. Provide user name, password
     * to autenticate with the referred to directory server.
     */
    static class ReferralRebindProc implements LDAPRebind {
        LDAPRebindAuth auth;
        
        public ReferralRebindProc(String user, String passwd) {
            auth = new LDAPRebindAuth(user, passwd);
        }    
        
        public LDAPRebindAuth getRebindAuthentication(String host, int port) {
            return auth;
        }
    }

    /**
     * Set maximal number of referral hops under default SearchConstraints for the
     * connection.
     * Specified with the env property java.naming.referral.limit
     */
    void updateReferralHopLimit(LDAPSearchConstraints cons) throws IllegalArgumentException{
        String limit = (String)getProperty(P_REFERRAL_HOPLIMIT);
        if(limit != null) {
            int n = -1;
            try {
                n = Integer.parseInt(limit);
            }
            catch (Exception e) {
                throw new IllegalArgumentException("Illegal value for " + P_REFERRAL_HOPLIMIT);
            }
            cons.setHopLimit(n);
        }
    }


    /**
     * Check if SSL mode is enabled
     */
    boolean isSSLEnabled() throws NamingException {
        String secMode = (String)getProperty(P_SECURITY_PROTOCOL);
        if(secMode != null) {
            if(secMode.equalsIgnoreCase("ssl")) {
                return true;
            }
            else {
                throw new AuthenticationNotSupportedException(
                "Unsupported value for " + P_SECURITY_PROTOCOL);
            }
        }
        return false;
    }

    /**
     * Get the Directory Server URL
     */
    LDAPUrl getDirectoryServerURL() throws NamingException{
        String url =  (String) getProperty(Context.PROVIDER_URL);
        try {
            return (url == null) ? null : new LDAPUrl(url);
        }
        catch (java.net.MalformedURLException e) {
            throw new IllegalArgumentException(
            "Illegal value for " + Context.PROVIDER_URL);
        }
    }

    /**
     * Get Ldap Version. If not specified the default is version 3
     */
    int getLdapVersion() throws NamingException{
        String version = (String) getProperty(P_LDAP_VERSION);
        if (version != null) {
            int v = -1;
            try {
                v = Integer.parseInt(version);
            }
            catch (Exception e) {
                throw new IllegalArgumentException(
                "Illegal value for java.naming.ldap.version property.");
            }
            /*if ( v !=2 && v !=3) { 
                throw new IllegalArgumentException(
                "Illegal value for + java.naming.ldap.version property.");
            }BLITS*/
            return v;
        }
        return DEFAULT_LDAP_VERSION;
    }        
    
    /**
     * Get user authenticate name
     */
    String getUserDN() {
        return (String) getProperty(Context.SECURITY_PRINCIPAL);
    }        

    /**
     * Get user authenticate password
     */
    String getUserPassword() {
        return (String) getProperty(Context.SECURITY_CREDENTIALS);
    }    

    /**
     * Get full qualified socket factory class name
     */
    String getSocketFactory() {
        return (String)getProperty(P_SOCKET_FACTORY);
    }    

    /**
     * Get cipher suite for the socket factory
     */
    Object getCipherSuite() {
        return getProperty(P_CIPHER_SUITE);
    }    

    /**
     * Get controls to be used during a connection request like ProxyAuth
     */
    LDAPControl[] getConnectControls() throws NamingException{        
        Control[] reqCtls = (Control[])getProperty(P_CONNECT_CTRLS);
        if (reqCtls != null) {
            LDAPControl[] ldapCtls = new LDAPControl[reqCtls.length];
            for (int i=0; i < reqCtls.length; i++) {
                try {
                    ldapCtls[i] = (LDAPControl) reqCtls[i];
                }
                catch (ClassCastException ex) {
                    throw new NamingException(
                    "Unsupported control type " + reqCtls[i].getClass().getName());
                }
            }
            return ldapCtls;
        }
        return null;
    }

    /**
     * Flag whether search operation should return attribute names only
     * (no values). Read environment property P_ATTRS_ONLY. If not defined
     * FALSE is returned (return attribute values by default)
     */
    boolean getAttrsOnlyFlag() {
        String flag = (String)getProperty(P_ATTRS_ONLY);
        if (flag == null) {
            return false; //default
        }
        else if (flag.equalsIgnoreCase("true")) {
            return true;
        }    
        else if (flag.equalsIgnoreCase("false")) {
            return false;
        }    
        else {
            throw new IllegalArgumentException("Illegal value for " + P_ATTRS_ONLY);
        }
    }    

    /**
     * Flag whether rename operation should delete old RDN
     * Read environment property P_ATTRS_ONLY. If not defined
     * TRUE is returned (delete old RDN by default)     
     */
    boolean getDeleteOldRDNFlag() {
        String flag = (String)getProperty(P_DELETE_OLDRDN);
        if (flag == null) {
            return true; //default
        }
        else if (flag.equalsIgnoreCase("true")) {
            return true;
        }    
        else if (flag.equalsIgnoreCase("false")) {
            return false;
        }    
        else {
            throw new IllegalArgumentException("Illegal value for " + P_DELETE_OLDRDN);
        }
    }

    /**
     * A user defined value for the separator for JNDI References.
     * The default value is '#'.
     * Read environment property P_JNDIREF_SEPARATOR.
     */
    char getRefSeparator() throws NamingException{
        String sep = (String)getProperty(P_JNDIREF_SEPARATOR );
        if(sep != null) {
            if (sep.length() !=1) {
                throw new IllegalArgumentException("Illegal value for " + P_JNDIREF_SEPARATOR);
            }            
            return sep.charAt(0);
        }
        return '#';
    }

    /**
     * A user defined list of names of binary attributes. This list augments the
     * default list of well-known binary attributes. List entries are space separated
     * Read environment property P_BINARY_ATTRS.
     */
    String[] getUserDefBinaryAttrs() {
        String binAttrList = (String)getProperty(P_BINARY_ATTRS);
        if (binAttrList == null) {
            return null;
        }
        
        StringTokenizer tok = new StringTokenizer(binAttrList);        
        String[] binAttrs = new String[tok.countTokens()];
        for (int i=0; tok.hasMoreTokens(); i++) {
            binAttrs[i] = tok.nextToken();
        }
        return binAttrs;
    }

    /**
     * Check if sasl auth mode is requested. If the value of auth property is
     * neither of (null, none, or simple) then assume it is a space separated
     * list of sasl mechanis names
     */
    String[] getSaslMechanisms() {
        String authMode = (String)getProperty(P_SECURITY_AUTHMODE);
        if(authMode != null) {
            if(authMode.equalsIgnoreCase(V_AUTH_NONE)) {
                return null;
            }
            else if (authMode.equalsIgnoreCase(V_AUTH_SIMPLE)) {
                 return null;
            }     
            else {
                // The value must be a space separated list of sasl
                // mechanism names
                StringTokenizer tok = new StringTokenizer(authMode);
                int cnt = tok.countTokens();
                String[] mechanisms = new String[cnt];
                for (int i=0; tok.hasMoreTokens(); i++) {
                    mechanisms[i] = tok.nextToken();
                }                                        
            }
        }
        return null;
    }

    /**
     * Returned all sasl properties (startwith javax.security.sasl) except
     * AUTHID and CALLBACK, as a Hashtable. AUTHID and CALLBACK as used 
     * directly as parameters to authenticate()
     * 
     */
    Hashtable getSaslProps() {
        Hashtable props = getAllProperties();
        Hashtable saslProps = new Hashtable();
        String prefixUpperCase = SASL_PROP_PREFIX.toUpperCase();
        
        for (Enumeration e = props.keys(); e.hasMoreElements();) {
            String key = (String) e.nextElement();
            if (key.startsWith(SASL_PROP_PREFIX) ||
                key.startsWith(prefixUpperCase)) {
                if (!key.equalsIgnoreCase(P_SASL_AUTHID) && 
                    !key.equalsIgnoreCase(P_SASL_CALLBACK)) {
                    saslProps.put(key, props.get(key));
                }                    
            }
        }
        return (saslProps.size() > 0) ? saslProps : null;

    }

    /**
     * Return DN to be used for sasl auth. Check first the P_SASL_AUTHID
     * property, then fallback to P_USERDN if not defined.    
     */
    String getSaslAuthId() {
        String id = (String)getProperty(P_SASL_AUTHID);
        if (id != null) {
            return id;
        }       
        return (String)getProperty(P_USER_DN);
    }        

    /**
     * Return the callback object for sasl, if specified
     */
    Object getSaslCallback() {
        return getProperty(P_SASL_CALLBACK);
    }
}   
