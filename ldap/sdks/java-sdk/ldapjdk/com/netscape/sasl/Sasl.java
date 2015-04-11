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
package com.netscape.sasl;

import java.util.Hashtable;
import javax.security.auth.callback.CallbackHandler;
import java.util.StringTokenizer;
import java.io.*;

/**
 * A static class for creating SASL clients and servers.
 *<p>
 * This class defines the policy of how to locate, load, and instantiate
 * SASL clients and servers. 
 * Currently, only the client methods are available.
 *<p>
 * For example, an application or library gets a SASL client by doing
 * something like:
 *<blockquote><pre>
 * SaslClient sc = Sasl.createSaslClient(mechanisms,
 *     authorizationId, protocol, serverName, props, callbackHandler);
 *</pre></blockquote>
 * It can then proceed to use the client create an authentication connection.
 * For example, an LDAP library might use the client as follows:
 *<blockquote><pre>
 * InputStream is = ldap.getInputStream();
 * OutputStream os = ldap.getOutputStream();
 * byte[] toServer = sc.createInitialResponse();
 * LdapResult res = ldap.sendBindRequest(dn, sc.getName(), toServer);
 * while (!sc.isComplete() && res.status == SASL_BIND_IN_PROGRESS) {
 *     toServer = sc.evaluateChallenge(res.getBytesFromServer());
 *     if (toServer != null) {
 *        res = ldap.sendBindRequest(dn, sc.getName(), toServer);
 *     }
 * }
 * if (sc.isComplete() && res.status == SUCCESS) {
 *     // Get the input and output streams; may be unchanged
 *     is = sc.getInputStream( is );
 *     os = sc.getOutputStream( os );
 *     // Use these streams from now on
 *     ldap.setInputStream( is );
 *     ldap.setOutputStream( os );
 * }
 *</pre></blockquote>
 *
 * IMPLEMENTATION NOTE: To use this class on JDK1.2, the caller needs:
 *<ul><tt>
 *<li>java.lang.RuntimePermission("getSecurityManager")
 *<li>java.lang.RuntimePermission("getClassLoader")
 *<li>java.util.PropertyPermission("javax.security.sasl.client.pkgs", "read");
 *</tt></ul>
 */
public class Sasl {
    private static SaslClientFactory clientFactory = null;
    static final boolean debug = false;

    // Cannot create one of these
    private Sasl() { 
    }

    /**
     * The property name containing a list of package names, separated by
     * '|'. Each package contains a class named <tt>ClientFactory</tt> that
     * implements the <tt>SaslClientFactory</tt> interface.
     * Its value is "javax.security.sasl.client.pkgs".
     */
    public static final String CLIENTPKGS = "javax.security.sasl.client.pkgs";

    /**
     * Creates a SaslClient using the parameters supplied.
     * The algorithm for selection is as follows:
     *<ol>
     *<li>If a factory has been installed via <tt>setSaslClientFactory()</tt>, 
     * try it first. If non-null answer produced, return it.
     *<li>The <tt>javax.security.sasl.client.pkgs</tt> property contains
     * a '|'-separated list of package names. Each package contains a
     * class named <tt>ClientFactory</tt>.  Load each factory
     * and try to create a <tt>SaslClient</tt>. 
     * Repeat this for
     * each package on the list until a non-null answer is produced.
     * If non-null answer produced, return it.
     *<li>Repeat previous step using the <tt>javax.security.sasl.client.pkgs</tt>
     * System property.
     *<li>If no non-null answer produced, return null.
     *</ol>
     *
     * @param mechanisms The non-null list of mechanism names to try. Each is the
     * IANA-registered name of a SASL mechanism. (e.g. "GSSAPI", "CRAM-MD5").
     * @param authorizationId The possibly null authorization ID to use. When
     * the SASL authentication completes successfully, the entity named
     * by authorizationId is granted access. 
     * @param protocol The non-null string name of the protocol for which
     * the authentication is being performed (e.g., "ldap").
     * @param serverName The non-null string name of the server to which
     * we are creating an authenticated connection.
     * @param props The possibly null properties to be used by the SASL
     * mechanisms to configure the authentication exchange. For example,
     * "javax.security.sasl.encryption.maximum" might be used to specify
     * the maximum key length to use for encryption.
     * @param cbh The possibly null callback handler to used by the SASL
     * mechanisms to get further information from the application/library
     * to complete the authentication. For example, a SASL mechanism might
     * require the authentication ID and password from the caller.
     *@return A possibly null <tt>SaslClient</tt> created using the parameters
     * supplied. If null, cannot find a <tt>SaslClientFactory</tt>
     * that will produce one.
     *@exception SaslException If cannot create a <tt>SaslClient</tt> because
     * of an error.
     */
    public static SaslClient createSaslClient(
        String[] mechanisms,
        String authorizationId,
        String protocol,
        String serverName,
        Hashtable props, 
        CallbackHandler cbh) throws SaslException {

        if (debug) {
            System.out.println("Sasl.createSaslClient");
        }
        SaslClient mech = null;

        // If factory has been set, try it first
        if (clientFactory != null) {
            mech = clientFactory.createSaslClient(
                mechanisms, authorizationId, 
                protocol, serverName, props, cbh);
        }

        // No mechanism produced
        if (mech == null) {
            String pkgs = (props == null) ? null :
                (String) props.get(CLIENTPKGS);

            // Try properties argument
            if (pkgs != null) {
                mech = loadFromPkgList(pkgs, mechanisms,
                                       authorizationId,
                                       protocol, serverName,
                                       props, cbh);
            }

            // Try system properties
            if (mech == null &&
                (pkgs = System.getProperty(CLIENTPKGS)) != null) {
                mech = loadFromPkgList(pkgs, mechanisms,
                                       authorizationId,
                                       protocol, serverName,
                                       props, cbh);
            }
        }
        return mech;
    }

    private static SaslClient loadFromPkgList(String pkgs,
                                              String[] mechanisms,
                                              String authorizationId,
                                              String protocol,
                                              String serverName,
                                              Hashtable props, 
                                              CallbackHandler cbh)
        throws SaslException {

        StringTokenizer packagePrefixIter = new StringTokenizer(pkgs, "|");
        SaslClient mech = null;
        SaslClientFactory fac = null;

        while (mech == null && packagePrefixIter.hasMoreTokens()) {

            String pkg = packagePrefixIter.nextToken().trim();
            String clsName = pkg + ".ClientFactory";
            if (debug) {
                System.out.println("Sasl.loadFromPkgList: " + clsName);
            }
            Class cls = null;
            try {
                cls = Class.forName(clsName);
            } catch (Exception e) {
                System.err.println( "Sasl.loadFromPkgList: " + e );
            }
            if (cls != null) {
                try {
                    fac = (SaslClientFactory) cls.newInstance();
                } catch (InstantiationException e) {
                    throw new SaslException(
                        "Cannot instantiate " + clsName);
                } catch (IllegalAccessException e) {
                    throw new SaslException(
                        "Cannot access constructor of " + clsName);
                }
                mech = fac.createSaslClient(mechanisms, authorizationId, 
                                            protocol, serverName, props,
                                            cbh);
            }
        }
        return mech;
    }

    /**
     * Sets the default <tt>SaslClientFactory</tt> to use.
     * This method sets <tt>fac</tt> to be the default factory.
     * It can only be called with a non-null value once per VM. 
     * If a factory has been set already, this method throws
     * <tt>IllegalStateException</tt>.
     * @param fac The possibly null factory to set. If null, doesn't
     * do anything.
     * @exception IllegalStateException If factory already set.
     */
    public static void setSaslClientFactory(SaslClientFactory fac) {
        if (clientFactory != null) {
            throw new IllegalStateException (
                "SaslClientFactory already defined");
        }
        SecurityManager security = System.getSecurityManager();
        if (security != null) {
            security.checkSetFactory();
        }
        clientFactory = fac;
    }
}
