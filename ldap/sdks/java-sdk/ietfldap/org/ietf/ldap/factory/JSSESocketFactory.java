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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

package org.ietf.ldap.factory;

import java.io.IOException;
import java.io.Serializable;
import java.net.Socket;
import java.net.UnknownHostException;
import javax.net.SocketFactory;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.SSLSocketFactory;

import org.ietf.ldap.*;

/**
 * Creates an SSL socket connection to a server, using the JSSE package
 * from Sun. This class implements the <CODE>LDAPSocketFactory</CODE>
 * interface.
 * <P>
 *
 * @version 1.0
 * @see LDAPSocketFactory
 * @see LDAPConnection#LDAPConnection(org.ietf.ldap.LDAPSocketFactory)
 */
public class JSSESocketFactory
             implements LDAPSocketFactory, Serializable {

    static final long serialVersionUID = 6834205777733266609L;

    protected SSLSocketFactory factory = null;

    // Optional explicit cipher suites to use
    protected String[] suites = null;

    /**
     * Default factory constructor
     */
    public JSSESocketFactory() {
        this.factory = (SSLSocketFactory) SSLSocketFactory.getDefault();
    }

    /**
     * Factory constructor that uses the default JSSE SSLSocketFactory
     *
     * @param suites Cipher suites to attempt to use with the server;
     * if <code>null</code>, use any cipher suites available in the
     * JSSE package
     */
    public JSSESocketFactory( String[] suites ) {
        this.suites = suites;
        this.factory = (SSLSocketFactory)SSLSocketFactory.getDefault();
    }

    /**
    * Factory constructor
    * @param sf the SSL socketfactory to use
    */
    public JSSESocketFactory( SSLSocketFactory factory) {
        this.factory = factory;
    }
  
   /**
    * Factory constructor
    *
    * @param suites Cipher suites to attempt to use with the server;
    * if <code>null</code>, use any cipher suites available in the
    * JSSE package
    * @param sf the SSL socketfactory to use
    */
    public JSSESocketFactory( String[] suites, SSLSocketFactory factory ) {
        this.suites = suites;
        this.factory = factory;
    }

    /**
     * Creates an SSL socket
     *
     * @param host Host name or IP address of SSL server
     * @param port Port numbers of SSL server
     * @return A socket for an encrypted session
     * @exception LDAPException on error creating socket
     */
    public Socket createSocket(String host, int port)
        throws LDAPException { 
  
        SSLSocket sock = null;

        try {
            sock = (SSLSocket)factory.createSocket(host, port);

            if (suites != null) {
                sock.setEnabledCipherSuites(suites);
            }
            
            // Start handshake manually to immediately expose potential
            // SSL errors as exceptions. Otherwise, handshake will take
            // place first time the data are written to the socket.
            sock.startHandshake();

        } catch (UnknownHostException e) {
            throw new LDAPException("SSL connection to " + host +
                                    ":" + port + ", " + e.getMessage(),
                                    LDAPException.CONNECT_ERROR);
        } catch (IOException f) {
            throw new LDAPException("SSL connection to " + host +
                                    ":" + port + ", " + f.getMessage(),
                                    LDAPException.CONNECT_ERROR);
        }

        return sock;
    }
}

