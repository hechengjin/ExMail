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
import java.util.*;
import java.io.*;
import com.netscape.jndi.ldap.common.*;
import com.netscape.jndi.ldap.schema.SchemaRoot;

/**
 * Ldap Service encapsulates a Ldap connection and Ldap operations over the
 * connection. The connection is established in a lazy manner, first time a
 * Ldap operation is initiated. A Ldap Service object is shared by multiple
 * contexts. The object maintains a reference counter which is incremented
 * when a context is cloned, and decremeneted when a context is closed. The
 * associated Ldap Connection is relased when the reference counter reaches
 * zero.
 *
 * LDAPsearchConstraints are always read from a context, because ldap service
 * is a shared object and each context may request different search constraints.
 *
 */
class LdapService {

    public static final String DEFAULT_FILTER = "(objectclass=*)";
    public static final int    DEFAULT_SCOPE = LDAPv3.SCOPE_SUB;
    public static final String DEFAULT_HOST = "localhost";
    public static final int    DEFAULT_PORT = LDAPv2.DEFAULT_PORT;    
    public static final int    DEFAULT_SSL_PORT = 636;
    
    private LDAPConnection m_ld;
    private EventService m_eventSvc;
    
    /**
     * The number of contexts that are currently sharing this LdapService.
     * Essentially, a reference counter. Incremented in the LdapContextImpl
     * copy constructor. Decremented in the LdapService.disconnect().
     * When the count reaches zero, the LDAPConnection is released.
     */
    private int m_clientCount;
    
        
    public LdapService() {
        m_ld = new LDAPConnection();
        m_clientCount = 1;
    }

    LDAPConnection getConnection() {
        return m_ld;
    }

    /**
     * Connect to the server and send bind request to authenticate the user
     */
    void connect(LdapContextImpl ctx) throws NamingException{

        if (m_ld.isConnected()) {
            return; //already connected
        }
        
        LDAPUrl url = ctx.m_ctxEnv.getDirectoryServerURL();
        String host = (url != null) ? url.getHost() : DEFAULT_HOST;
        int    port = (url != null) ? url.getPort() : DEFAULT_PORT;
        String user   = ctx.m_ctxEnv.getUserDN();
        String passwd = ctx.m_ctxEnv.getUserPassword();
        String socketFactory = ctx.m_ctxEnv.getSocketFactory();
        Object cipherSuite   = ctx.m_ctxEnv.getCipherSuite();
        int    ldapVersion   = ctx.m_ctxEnv.getLdapVersion();
        boolean isSSLEnabled = ctx.m_ctxEnv.isSSLEnabled();
        String[] saslMechanisms = ctx.m_ctxEnv.getSaslMechanisms();
        LDAPControl[] ldCtrls= ctx.m_ctxEnv.getConnectControls();
        Object traceOut = ctx.m_ctxEnv.getProperty(ContextEnv.P_TRACE);

        // Set default ssl port if DS not specifed
        if (isSSLEnabled && url == null) {
            port = DEFAULT_SSL_PORT;
        }

        // SSL is enabled but no Socket factory specified. Use the
        // ldapjdk default one        
        if (isSSLEnabled && socketFactory == null) {
            m_ld = new LDAPConnection(new LDAPSSLSocketFactory());
        }    

        // Set the socket factory and cipher suite if cpecified
        if (socketFactory != null) {
            try {
                LDAPSSLSocketFactory sf = null;
                if (cipherSuite != null) {
                    Debug.println(1, "CIPHERS=" + cipherSuite);
                    sf = new LDAPSSLSocketFactory(socketFactory, cipherSuite);
                }
                else {
                    sf = new LDAPSSLSocketFactory(socketFactory);
                }    
                m_ld = new LDAPConnection(sf);
                Debug.println(1, "SSL CONNECTION");
            }
            catch (Exception e) {
                throw new IllegalArgumentException(
                "Illegal value for java.naming.ldap.factory.socket: " + e);
            }
        }

        // Enable tracing 
        if (traceOut != null) {
            setTraceOutput(traceOut);
        }
        
        try {
            if (ldCtrls != null) {
                m_ld.setOption(LDAPv3.SERVERCONTROLS, ldCtrls);
            }
            
            if (saslMechanisms != null) { // sasl Auth
                m_ld.authenticate(ctx.m_ctxEnv.getSaslAuthId(),
                                  saslMechanisms,
                                  ctx.m_ctxEnv.getSaslProps(),
                                  ctx.m_ctxEnv.getSaslCallback());
            }
            else { // simple auth
                m_ld.connect(ldapVersion, host, port, user, passwd);
            }                
        }
        catch (LDAPException e) {            
           // If ldapVersion is not specified in ctx env
           // fallback to ldap v2 if v3 is bot supported
            if (e.getLDAPResultCode() == e.PROTOCOL_ERROR &&
                ldapVersion == 3 &&
                saslMechanisms == null &&
                ctx.m_ctxEnv.getProperty(
                     ctx.m_ctxEnv.P_LDAP_VERSION) == null) {
                
                try {
                    m_ld.connect(2, host, port, user, passwd);
                }
                catch (LDAPException e2) {
                    throw ExceptionMapper.getNamingException(e2);
                }
            }
            else {
                throw ExceptionMapper.getNamingException(e);
            }                
        }
    }

    protected void finalize() {
        try {
            m_ld.disconnect();
        }
        catch (Exception e) {}
    }

    boolean isConnected() {
        return (m_ld.isConnected());
    }
    

    /**
     * Physically disconect only if the client count is zero
     */
    synchronized void disconnect() {
        try {
            if (m_clientCount > 0) {
                m_clientCount--;
            }    
            if (m_clientCount == 0 && isConnected()) {
                m_ld.disconnect();
            }
        }
        catch (Exception e) {}
    }    

    /**
     * Increment client count
     */
    synchronized void incrementClientCount() {
        m_clientCount++;
    }
        
    /**
     * LDAP search operation
     */
    NamingEnumeration search (LdapContextImpl ctx, String name, String filter, String[] attrs, SearchControls jndiCtrls) throws NamingException{
        Debug.println(1, "SEARCH");
        String base = ctx.getDN();
        int scope  = LDAPConnection.SCOPE_SUB;
        LDAPSearchConstraints cons=ctx.getSearchConstraints();
        boolean returnObjs = false;
                
        connect(ctx); // Lazy Connect

        // Create DN by appending the name to the current context
        if (name.length() > 0) {
            if (base.length() > 0) {
                base = name + "," + base;
            }
            else {
                base = name;
            }
        }
                
        // Map JNDI SearchControls to ldapjdk LDAPSearchConstraints
        if (jndiCtrls != null) {
            int maxResults = (int)jndiCtrls.getCountLimit();
            int timeLimitInMsec = jndiCtrls.getTimeLimit();
            // Convert timeLimit in msec to sec 
            int timeLimit = timeLimitInMsec/1000;
            if (timeLimitInMsec > 0 && timeLimitInMsec < 1000) {
                timeLimit = 1; //sec
            }
            
            // Clone cons if maxResults or timeLimit is different from the default one
            if (cons.getServerTimeLimit() != timeLimit || cons.getMaxResults() != maxResults) {
                cons = (LDAPSearchConstraints)cons.clone();
                cons.setMaxResults(maxResults);
                cons.setServerTimeLimit(timeLimit);
            }    
            
            // TODO The concept of links is not well described in JNDI docs.
            // We can only specify dereferencing of Aliases, but Links and
            // Aliases are not the same; IGNORE jndiCtrls.getDerefLinkFlag()
        
            // Attributes to return
            attrs = jndiCtrls.getReturningAttributes();
            if (attrs != null && attrs.length==0) {
                //return no attributes
                attrs = new String[] { "1.1" };
               
               // TODO check whether ldap compare operation should be performed
               // That's the case if: (1) scope is OBJECT_SCOPE, (2) no attrs
               // are requested to return (3) filter has the form (name==value)
               // where no wildcards ()&|!=~><* are used.
                
            }            
            
            // Search scope
            scope = ProviderUtils.jndiSearchScopeToLdap(jndiCtrls.getSearchScope());

            // return obj flag
            returnObjs = jndiCtrls.getReturningObjFlag();
        }    
            
        try {
            // Perform Search
            boolean attrsOnly  = ctx.m_ctxEnv.getAttrsOnlyFlag();
            LDAPSearchResults res = m_ld.search( base, scope, filter, attrs, attrsOnly, cons );
            return new SearchResultEnum(res, returnObjs, ctx);
        }
        catch (LDAPReferralException e) {
            throw new LdapReferralException(ctx, e);
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }
    
    /**
     * List child entries using LDAP lookup operation
     */
    NamingEnumeration listEntries(LdapContextImpl ctx, String name, boolean returnBindings) throws NamingException{
        Debug.println(1, "LIST " + (returnBindings ? "BINDINGS" : ""));
        String base = ctx.getDN();

        connect(ctx); // Lazy Connect
        
        // Create DN by appending the name to the current context
        if (name.length() > 0) {
            if (base.length() > 0) {
                base = name + "," + base;
            }
            else {
                base = name;
            }
        }
            
        try {
            // Perform One Level Search
            String[] attrs = null; // return all attributes if Bindings are requested 
            if (!returnBindings) { // for ClassNamePairs
                attrs = new String[]{"javaclassname"}; //attr names must be in lowercase
            }
            LDAPSearchConstraints cons=ctx.getSearchConstraints();
            LDAPSearchResults res = m_ld.search( base, LDAPConnection.SCOPE_ONE, DEFAULT_FILTER, attrs, /*attrsOnly=*/false, cons);
            if (returnBindings) {
                return new BindingEnum(res, ctx);
            }
            else {
                return new NameClassPairEnum(res, ctx);
            }
        }
        catch (LDAPReferralException e) {
            throw new LdapReferralException(ctx, e);
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    /**
     * Lookup an entry using LDAP search operation
     */
    Object lookup(LdapContextImpl ctx, String name) throws NamingException{
        Debug.println(1, "LOOKUP");        
        String base = ctx.getDN();

        connect(ctx); // Lazy Connect

        // Create DN by appending the name to the current context
        if (name.length() > 0) {
            if (base.length() > 0) {
                base = name + "," + base;
            }
            else {
                base = name;
            }
        }
            
        try {
            // Perform Base Search
            String[] attrs = null; // return all attrs
            LDAPSearchConstraints cons=ctx.getSearchConstraints();
            LDAPSearchResults res = m_ld.search( base, LDAPConnection.SCOPE_BASE, DEFAULT_FILTER, attrs, /*attrsOnly=*/false, cons);
            if (res.hasMoreElements()) {
                LDAPEntry entry = res.next();
                return ObjectMapper.entryToObject(entry, ctx);
            }
            return null; // should never get here

        }
        catch (LDAPReferralException e) {
            throw new LdapReferralException(ctx, e);
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    /**
     * Read LDAP entry attributes
     */
    Attributes readAttrs (LdapContextImpl ctx, String name, String[] attrs) throws NamingException{
        Debug.println(1, "READ ATTRS");
        String base = ctx.getDN();
        int scope  = LDAPConnection.SCOPE_BASE;
        
        connect(ctx); // Lazy Connect

        // Create DN by appending the name to the current context
        if (name.length() > 0) {
            if (base.length() > 0) {
                base = name + "," + base;
            }
            else {
                base = name;
            }
        }

        try {
            // Perform Search
            LDAPSearchConstraints cons=ctx.getSearchConstraints();
            LDAPSearchResults res = m_ld.search(base, scope, DEFAULT_FILTER, attrs, /*attrsOnly=*/false, cons);
            while (res.hasMoreElements()) {
                LDAPEntry entry = res.next();
                return new AttributesImpl(entry.getAttributeSet(),
                       ctx.m_ctxEnv.getUserDefBinaryAttrs());
            }
            return null;
        }
        catch (LDAPReferralException e) {
            throw new LdapReferralException(ctx, e);
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    /**
     * Modify LDAP entry attributes
     */
    void modifyEntry(LdapContextImpl ctx, String name, LDAPModificationSet mods) throws NamingException{
        Debug.println(1, "MODIFY");
        String base = ctx.getDN();
        
        if (mods.size() == 0) {
            return;
        }
        connect(ctx); // Lazy Connect

        // Create DN by appending the name to the current context
        if (name.length() > 0) {
            if (base.length() > 0) {
                base = name + "," + base;
            }
            else {
                base = name;
            }
        }
        
        try {
            // Perform Modify
            m_ld.modify(base, mods);
        }
        catch (LDAPReferralException e) {
            throw new LdapReferralException(ctx, e);
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    /**
     * Create a new LDAP entry
     */
    LdapContextImpl addEntry(LdapContextImpl ctx, String name, LDAPAttributeSet attrs) throws NamingException{
        Debug.println(1, "ADD");        
        String dn = ctx.getDN();
        
        connect(ctx); // Lazy Connect

        // Create DN by appending the name to the current context
        if (name.length() == 0) {
            throw new IllegalArgumentException("Name can not be empty");
        }    
        if (dn.length() > 0) {
            dn = name + "," + dn;
        }
        else {
            dn = name;
        }
        
        try {
            // Add a new entry
            m_ld.add(new LDAPEntry(dn, attrs));
            return new LdapContextImpl(dn, ctx);
        }
        catch (LDAPReferralException e) {
            throw new LdapReferralException(ctx, e);
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    /**
     * Delete a LDAP entry
     */
    void delEntry(LdapContextImpl ctx, String name) throws NamingException{
        Debug.println(1, "DELETE");
        String dn = ctx.getDN();
        
        connect(ctx); // Lazy Connect

        // Create DN by appending the name to the current context
        if (name.length() == 0) {
            throw new IllegalArgumentException("Name can not be empty");
        }    
        if (dn.length() > 0) {
            dn = name + "," + dn;
        }
        else {
            dn = name;
        }
        
        try {
            // Perform Delete
            m_ld.delete(dn);
        }
        catch (LDAPReferralException e) {
            throw new LdapReferralException(ctx, e);
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    /**
     * Chanage RDN for a LDAP entry
     */
    void changeRDN(LdapContextImpl ctx, String name, String newRDN) throws NamingException{
        Debug.println(1, "RENAME");
        String dn = ctx.getDN();
        
        connect(ctx); // Lazy Connect

        // Create DN by appending the name to the current context
        if (name.length() == 0 || newRDN.length() == 0) {
            throw new IllegalArgumentException("Name can not be empty");
        }    
        if (dn.length() > 0) {
            dn = name + "," + dn;
        }
        else {
            dn = name;
        }

        try {
            // Rename
            m_ld.rename(dn, newRDN, ctx.m_ctxEnv.getDeleteOldRDNFlag());            
        }
        catch (LDAPReferralException e) {
            throw new LdapReferralException(ctx, e);
        }
        catch (LDAPException e) {
            throw ExceptionMapper.getNamingException(e);
        }
    }

    /**
     * Schema Operations
     */
    DirContext getSchema(LdapContextImpl ctx, String name) throws NamingException {        
        connect(ctx); // Lazy Connect        
        return new SchemaRoot(m_ld);
    }

    /**
     * Return the event service
     */
    EventService getEventService(LdapContextImpl ctx) throws NamingException {
        connect(ctx); // Lazy Connect
        if (m_eventSvc == null) {
            m_eventSvc = new EventService(this);
        }    
        return m_eventSvc;
    }
    
    /**
     * Enable/Disable ldap message trace.
     * @param out Trace output or null (disable trace). Output can
     * be specified as a file name or a java OutputStream. If an
     * empty string is specified, the output is sent to System.err.
     * A file name prefixed with a '+' will open the file in append mode.
     */
    void setTraceOutput(Object out) throws NamingException {
        try {
            m_ld.setProperty(m_ld.TRACE_PROPERTY, out);
        }
        catch (Exception e) {
            throw new IllegalArgumentException("Can not open trace output " + e);
        }            
    }
}
