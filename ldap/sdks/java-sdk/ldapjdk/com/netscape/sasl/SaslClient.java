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
import java.io.*;

/**
 * Performs SASL authentication as a client.
 *<p>
 * A protocol library such as one for LDAP gets an instance of this
 * class in order to perform authentication defined by a specific SASL
 * mechanism. Invoking methods on the <tt>SaslClient</tt> instance
 * process challenges and create responses according to the SASL
 * mechanism implemented by the <tt>SaslClient</tt>.
 * As the authentication proceeds, the instance
 * encapsulates the state of a SASL client's authentication exchange. 
 *<p>
 * Here's an example of how an LDAP library might use a <tt>SaslClient</tt>.
 * It first gets an instance of a <tt>SaslClient</tt>:
 *<blockquote><pre>
 * SaslClient sc = Sasl.createSaslClient(mechanisms,
 *     authorizationId, protocol, serverName, props, callbackHandler);
 *</pre></blockquote>
 * It can then proceed to use the client for authentication.
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
 * Note that the call to <tt>createInitialResponse()</tt> is optional.
 * Protocols such as IMAP4 do not invoke it but instead only use
 * <tt>evaluateChallenge()</tt>, possibly with an empty challenge.
 * It is the responsibility of the <tt>SaslClient</tt> implementation
 * for a mechanism to take this into account so that it behaves properly
 * regardless of whether <tt>createInitialResponse()</tt> is called.
 *
 * @see Sasl
 * @see SaslClientFactory
 */
public abstract interface SaslClient {

    /**
     * Returns the IANA-registered mechanism name of this SASL client.
     * (e.g. "CRAM-MD5", "GSSAPI").
     * @return A non-null string representing the IANA-registered mechanism name.
     */
    public abstract String getMechanismName();

    /**
     * Retrieves the initial response.
     *
     * @return The possibly null byte array containing the initial response.
     * It is null if the mechanism does not have an initial response.
     * @exception SaslException If an error occurred while creating
     * the initial response.
     */
    public abstract byte[] createInitialResponse() throws SaslException;

    /**
     * Evaluates the challenge data and generates a response.
     *
     * @param challenge The non-null challenge sent from the server.
     *
     * @return The possibly null reponse to send to the server.
     * It is null if the challenge accompanied a "SUCCESS" status and the challenge
     * only contains data for the client to update its state and no response
     * needs to be sent to the server.
     * @exception SaslException If an error occurred while processing
     * the challenge or generating a response.
     */
    public abstract byte[] evaluateChallenge(byte[] challenge) 
	throws SaslException;

    /**
      * Determines whether the authentication exchange has completed.
      * @return true if the authentication exchange has completed; false otherwise.
      */
    public abstract boolean isComplete();

    /**
     * Retrieves an input stream for the session. It may return
	 * the same stream that is passed in, if no processing is to be
	 * done by the client object.
     *
     * This method can only be called if isComplete() returns true.
     * @param is The original input stream for reading from the server.
     * @return An input stream for reading from the server, which
	 * may include processing the original stream.
     * @exception IOException If the authentication exchange has not completed
     * or an error occurred while getting the stream.
     */
    public abstract InputStream getInputStream(InputStream is) throws IOException;

    /**
     * Retrieves an output stream for the session. It may return
	 * the same stream that is passed in, if no processing is to be
	 * done by the client object.
     *
     * This method can only be called if isComplete() returns true.
     * @param os The original output stream for writing to the server.
     * @return An output stream for writing to the server, which
	 * may include processing the original stream.
     * @exception IOException If the authentication exchange has not completed
     * or an error occurred while getting the stream.
     */
    public abstract OutputStream getOutputStream(OutputStream os) throws IOException;
}
