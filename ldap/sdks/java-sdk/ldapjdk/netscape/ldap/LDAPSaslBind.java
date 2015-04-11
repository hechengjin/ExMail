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
import netscape.ldap.ber.stream.*;
import netscape.ldap.util.*;
import java.io.*;
import java.net.*;
//import javax.security.auth.callback.CallbackHandler;

/**
 * Authenticates to a server using SASL
 */
public class LDAPSaslBind implements LDAPBind, java.io.Serializable {

    static final long serialVersionUID = -7615315715163655443L;

    /**
     * Construct an object which can authenticate to an LDAP server
     * using the specified name and a specified SASL mechanism.
     *
     * @param dn if non-null and non-empty, specifies that the connection and
     * all operations through it should authenticate with dn as the
     * distinguished name
     * @param mechanisms array of mechanism names, e.g. { "GSSAPI", "SKEY" }
     * @param props optional additional properties of the desired
     * authentication mechanism, e.g. minimum security level
     * @param cbh a class which may be called by the SASL framework to
     * obtain additional required information
     */
    public LDAPSaslBind( String dn,
                         String[] mechanisms,
                         String packageName,
                         Hashtable props,
                         /*CallbackHandler*/ Object cbh ) {
        _dn = dn;
        _mechanisms = mechanisms;
        _packageName = packageName;
        _props = props;
        _cbh = cbh;

        // 12-01-99 Disabled check for instanceof CallbackHandler so that
        // there is no dependency on the extenal jaas.jar package. This is
        // reqired for Communicator build where the ldap java package does not
        // include any sasl classes.

        /*if ( (cbh != null) &&
             !(cbh instanceof javax.security.auth.callback.CallbackHandler) ) {
            throw new IllegalArgumentException(
                "Callback argument must implement " +
                "javax.security.auth.callback.CallbackHandler" );
        }*/
    }

    /**
     * Authenticates to the LDAP server (that the object is currently
     * connected to) using the parameter that were provided to the
     * constructor. If the requested SASL mechanism is not
     * available, an exception is thrown.  If the object has been
     * disconnected from an LDAP server, this method attempts to reconnect
     * to the server. If the object had already authenticated, the old
     * authentication is discarded.
     *
     * @param ldc an active connection to a server, which will have
     * the new authentication state on return from the method
     * @exception LDAPException Failed to authenticate to the LDAP server.
     */
    public void bind( LDAPConnection ldc ) throws LDAPException {
        if ( _props == null ) {
            _props = new Hashtable();
        }
        if ( (!_props.containsKey( CLIENTPKGS )) &&
             (System.getProperty( CLIENTPKGS ) == null) ) {
            _props.put( CLIENTPKGS, ldc.DEFAULT_SASL_PACKAGE );
        }
        _saslClient = getClient( ldc, _packageName );
        if ( _saslClient != null ) {
            bind( ldc, true );
            return;
        } else {
            ldc.printDebug( "LDAPSaslBind.bind: getClient " +
                            "returned null" );
        }
    }

    /**
     * Get a SaslClient object from the Sasl framework
     *
     * @param ldc contains the host name
     * @param packageName package containing a ClientFactory
     * @return a SaslClient supporting one of the mechanisms
     * of the member variable _mechanisms.
     * @exception LDAPException on error producing a client
     */
    private Object getClient( LDAPConnection ldc, String packageName )
        throws LDAPException {
        try {
            Object[] args = new Object[6];
            args[0] = _mechanisms;
            args[1] = _dn;
            args[2] = "ldap";
            args[3] = ldc.getHost();
            args[4] = _props;
            args[5] = _cbh;
            String[] argNames = new String[6];
            argNames[0] = "[Ljava.lang.String;";
            argNames[1] = "java.lang.String";
            argNames[2] = "java.lang.String";
            argNames[3] = "java.lang.String";
            argNames[4] = "java.util.Hashtable";
            argNames[5] = CALLBACK_HANDLER;

            // Get a mechanism driver
            return DynamicInvoker.invokeMethod(null,
                                               packageName+".Sasl",
                                               "createSaslClient",
                                               args, argNames);

        } catch (Exception e) {
            ldc.printDebug( "LDAPSaslBind.getClient: " +
                            packageName+".Sasl.createSaslClient: " +
                            e );
            throw new LDAPException(e.toString(), LDAPException.OTHER);
        }
    }

    void bind(LDAPConnection ldc, boolean rebind)
        throws LDAPException {

        if ((ldc.isConnected() && rebind) ||
            !ldc.isConnected()) {
            try {
                // Get the initial request to start authentication
                String className = _saslClient.getClass().getName();
                ldc.printDebug( "LDAPSaslBind.bind: calling " +
                                className+".createInitialResponse" );
                byte[] outVals =
                    (byte[])DynamicInvoker.invokeMethod(
                        _saslClient,
                        className,
                        "createInitialResponse", null, null);

                String mechanismName =
                    (String)DynamicInvoker.invokeMethod(
                        _saslClient,
                        className,
                        "getMechanismName", null, null);
                ldc.printDebug( "LDAPSaslBind.bind: mechanism " +
                                "name is " +
                                mechanismName );
                boolean isExternal = isExternalMechanism(mechanismName);
                int resultCode = LDAPException.SASL_BIND_IN_PROGRESS;
                JDAPBindResponse response = null;
                while (!checkForSASLBindCompletion(resultCode)) {
                    ldc.printDebug( "LDAPSaslBind.bind: calling " +
                                    "saslBind" );
                    response = saslBind(ldc, mechanismName, outVals);
                    resultCode = response.getResultCode();
                    ldc.printDebug( "LDAPSaslBind.bind: saslBind " +
                                    "returned " + resultCode );
                    if (isExternal) {
                        continue;
                    }

                    byte[] b = response.getCredentials();

                    Object[] args = {b};
                    String[] argNames = {"[B"}; // class name for byte array

                    outVals =
                        (byte[])DynamicInvoker.invokeMethod(
                            _saslClient,
                            className, "evaluateChallenge",
                            args, argNames);
                }

                // Make sure authentication REALLY is complete
                Boolean bool =
                    (Boolean)DynamicInvoker.invokeMethod(
                        _saslClient,
                        className, "isComplete", null, null);
                if (!bool.booleanValue()) {
                    // Authentication session hijacked!
                    throw new LDAPException("The server indicates that " +
                                            "authentication is successful" +
                                            ", but the SASL driver " +
                                            "indicates that authentication" +
                                            " is not yet done.",
                                            LDAPException.OTHER);
                }

                Object[] args = {ldc.getInputStream()};
                String[] argNames = {"java.io.InputStream"};
                InputStream is =
                    (InputStream)DynamicInvoker.invokeMethod(
                        _saslClient,
                        className, "getInputStream", args, argNames);
                ldc.setInputStream(is);
                args[0] = ldc.getOutputStream();
                argNames[0] = "java.io.OutputStream";
                OutputStream os =
                    (OutputStream)DynamicInvoker.invokeMethod(
                        _saslClient,
                        className, "getOutputStream", args, argNames);
                ldc.setOutputStream(os);
                ldc.setBound(true);
            } catch (LDAPException e) {
                throw e;
            } catch (Exception e) {
                throw new LDAPException(e.toString(), LDAPException.OTHER);
            }
        }
    }

    boolean isExternalMechanism(String name) {
        return name.equalsIgnoreCase( LDAPConnection.EXTERNAL_MECHANISM );
    }

    private boolean checkForSASLBindCompletion(int resultCode)
      throws LDAPException{

        if (resultCode == LDAPException.SUCCESS) {
            return true;
        } else if (resultCode == LDAPException.SASL_BIND_IN_PROGRESS) {
            return false;
        } else {
            throw new LDAPException("Authentication failed", resultCode);
        }
    }

    private JDAPBindResponse saslBind(LDAPConnection ldc,
                                      String mechanismName,
                                      byte[] credentials)
        throws LDAPException {

        LDAPResponseListener myListener = ldc.getResponseListener ();

        try {
            ldc.sendRequest(new JDAPBindRequest(3,
                                                _dn,
                                                mechanismName,
                                                credentials),
                            myListener, ldc.getConstraints());
            LDAPMessage response = myListener.getResponse();

            JDAPProtocolOp protocolOp = response.getProtocolOp();
            if (protocolOp instanceof JDAPBindResponse) {
                return (JDAPBindResponse)protocolOp;
            } else {
                throw new LDAPException("Unknown response from the " +
                                        "server during SASL bind",
                                        LDAPException.OTHER);
            }
        } finally {
            ldc.releaseResponseListener(myListener);
        }
    }

    private static final String CALLBACK_HANDLER =
        "javax.security.auth.callback.CallbackHandler";
    private static final String CLIENTPKGS =
        "javax.security.sasl.client.pkgs";
    private String _dn;
    private String[] _mechanisms;
    private String _packageName;
    private Hashtable _props;
    private /*CallbackHandler*/ Object _cbh;
    private Object _saslClient = null;
}
