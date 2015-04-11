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
package netscape.ldap.ber.stream;

import java.util.*;
import java.io.*;

/**
 * This class is for the UTCTime object.
 *
 * <pre>
 * ENCODING RULE:
 *   Primitive Definite length.
 *   tag = 0x17
 *   length = (short or long form)
 *   one or more contents octets
 * </pre>
 *
 * @version 1.0
 * seeAlso CCITT X.209
 */
public class BERUTCTime extends BERElement {

    /**
     * Internal variables
     */
    private String m_value = null;

    /**
     * Constructs a UTC time element containing the specified string.
     * @param utc_string string in UTC time format
     */
    public BERUTCTime(String utc_string) {
        m_value = utc_string;
    }

    /**
     * Constructs a UTCTime element from an input stream
     * (for constructed encoding)
     * @param stream source
     * @param bytes_read array of 1 int, incremented by number of bytes read
     * @exception IOException failed to construct
     */
    public BERUTCTime(BERTagDecoder decoder, InputStream stream,
                          int[] bytes_read) throws IOException {
        int octet;
        int contents_length = super.readLengthOctets(stream, bytes_read);
        int[] component_length = new int[1];
        BERElement element = null;

        m_value = "";

        if (contents_length == -1) {
            /* Constructed - indefinite length content octets. */
            {
                component_length[0] = 0;
                element = getElement(decoder,stream,component_length);
                if (element != null) {
                    /* element is an octetstring - add it to the existing buffer */
                    BERUTCTime utc_element = (BERUTCTime)element;
                    m_value += utc_element.getValue();
                }
            } while (element != null);
        } else {
            /* Definite length content octets string. */
            bytes_read[0] += contents_length;
            while (contents_length > 0) {
                component_length[0] = 0;
                element = getElement(decoder,stream,component_length);
                if (element != null) {
                    /* element is an octetstring - add it to the existing buffer */
                    BERUTCTime utc_element = (BERUTCTime)element;
                    m_value += utc_element.getValue();
                }
                contents_length -= component_length[0];
            }
        }
    }

    /**
     * Constructs a UTC time element from an input stream
     * (for primitive encoding)
     * @param stream source
     * @param bytes_read array of 1 int, incremented by number of bytes read
     * @exception IOException failed to construct
     */
    public BERUTCTime(InputStream stream, int[] bytes_read) throws IOException {
        int contents_length = super.readLengthOctets(stream, bytes_read);

        /* Definite length content octets string. */
        if (contents_length > 0) {
            byte[] byte_buf = new byte[contents_length];
            for (int i = 0; i < contents_length; i++) {
                byte_buf[i] = (byte) stream.read();
            }
            bytes_read[0] += contents_length;
            try{
                m_value = new String(byte_buf, "UTF8");
            } catch(Throwable x)
            {}
        }
    }

    private byte[] byte_buf;
    /**
     * Writes BER to a stream.
     * @exception IOException failed to write
     */
    public void write(OutputStream stream) throws IOException {
        stream.write((byte)getType());
        if (m_value == null) {
            sendDefiniteLength(stream, 0);
        } else {

            try{
                byte_buf = m_value.getBytes("UTF8");
                sendDefiniteLength(stream, byte_buf.length);  /* length */
            } catch(Throwable x)
            {}
            stream.write(byte_buf,0,byte_buf.length);  /* contents */
        }
    }

    /**
     * Gets the element value.
     */
    public String getValue() {
        return m_value;
    }

    /**
     * Gets the element type.
     */
    public int getType() {
        return BERElement.UTCTIME;
    }

    /**
     * Gets the string representation.
     * NOTE: currently prints out values in decimal form.
     * @return string representation of tag.
     */
    public String toString() {
        if (m_value == null)
            return "UTCTime (null)";
        else
            return "UTCTime {" + m_value + "}";
    }
}
