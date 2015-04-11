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
 * This class is for the OctetString type.
 *
 * <pre>
 * ENCODING RULE:
 *   Primitive Definite length.
 *   tag = 0x04
 *   length = (short or long form)
 *   one or more contents octets
 * </pre>
 *
 * @version 1.0
 * seeAlso CCITT X.209
 */
public class BEROctetString extends BERElement {

    /**
     * Raw value of element
     */
    private byte[] m_value = null;

    /**
     * Constructs an octet string element containing a copy of
     * the contents of buffer.
     * @param buffer a UCS-2 String
     */
    public BEROctetString(String buffer) {
        if (buffer == null)
            return;

        try {
            m_value = buffer.getBytes( "UTF8" );
        } catch (Throwable xxx){};
    }

    /**
     * Constructs an octet string element containing a reference to
     * buffer.
     * @param buffer a byte array, which must be in UTF-8 format if
     * it is string data
     */
    public BEROctetString(byte[] buffer) {
        m_value = buffer;
    }

    /**
     * Constructs an octet string element containing a
     * subset of buffer.
     * @param buffer buffer containing 'octets'
     * @param start start of buffer range to copy
     * @param end end of buffer range to copy
     */
    public BEROctetString(byte[] buffer, int start, int end) {
        m_value = new byte[end - start];

        for (int i = 0; i < end - start; i++)
            m_value[i] = buffer[start + i];
    }

    /**
     * Constructs an octet string element from an input stream
     * (for constructed encoding)
     * @param decoder a decode that understands the specific tags
     * @param stream source
     * @param bytes_read array of 1 int, incremented by number of bytes read
     * @exception IOException failed to construct
     */
    public BEROctetString(BERTagDecoder decoder, InputStream stream,
                          int[] bytes_read) throws IOException {
        int octet;
        int contents_length = super.readLengthOctets(stream, bytes_read);
        int[] component_length = new int[1];
        BERElement element = null;

        if (contents_length == -1) {
            /* Constructed - indefinite length content octets. */
            do {
                component_length[0] = 0;
                element = getElement(decoder,stream,component_length);
                if (element != null) {
                    /* element is an octetstring - add it to the existing buffer */
                    BEROctetString octet_element = (BEROctetString)element;
                    byte[] octet_buffer = octet_element.getValue();
                    if (m_value == null) {
                        m_value = new byte[octet_buffer.length];
                        System.arraycopy(octet_buffer,0,
                                        m_value,0,octet_buffer.length);
                    } else {
                        byte[] new_buffer = new byte[m_value.length +
                                                   octet_buffer.length];
                        System.arraycopy(m_value,0,new_buffer,0,m_value.length);
                        System.arraycopy(octet_buffer,0,
                                        new_buffer,m_value.length,
                                        octet_buffer.length);
                        m_value = new_buffer;
                    }
                }
            } while (element != null);
        } else {
            /* Definite length content octets string. */
            bytes_read[0] += contents_length;
            m_value = new byte[contents_length];
            int cnt=0, idx=0;
            while (idx < contents_length) {
                cnt = stream.read(m_value, idx, contents_length-idx);
                idx += cnt;
            }
        }
    }

    /**
     * Constructs an octet string element from an input stream
     * (for primitive encoding)
     * @param stream source
     * @param bytes_read array of 1 int, incremented by number of bytes read
     * @exception IOException failed to construct
     */
    public BEROctetString(InputStream stream, int[] bytes_read)
        throws IOException {
        int contents_length = super.readLengthOctets(stream, bytes_read);

        /* Definite length content octets string. */
        if (contents_length > 0) {
            m_value = new byte[contents_length];
            for (int i = 0; i < contents_length; i++) {
                m_value[i] = (byte) stream.read();
            }
            bytes_read[0] += contents_length;
        }
    }

    /**
     * Writes BER to stream
     * @exception IOException failed to write
     */
    public void write(OutputStream stream) throws IOException {
        stream.write((byte)BERElement.OCTETSTRING);  /* OCTETSTRING tag */
        if (m_value == null) {
            sendDefiniteLength(stream, 0);
        } else {
            sendDefiniteLength(stream, m_value.length);  /* length */
          stream.write(m_value,0,m_value.length);  /* contents */
        }
    }

    /**
     * Gets the element value.
     */
    public byte[] getValue() {
        return m_value;
    }

    /**
     * Gets the element type.
     */
    public int getType() {
        return BERElement.OCTETSTRING;
    }

    /**
     * Gets the string representation.
     * NOTE: currently prints out values in decimal form.
     * @return string representation of tag.
     */
    public String toString() {
        if (m_value == null) {
            return "OctetString (null)";
        }
        StringBuffer octets = new StringBuffer("OctetString {");
        for (int i = 0; i < m_value.length; i++) {
            if (i != 0) {
                octets.append(' ');
            }
            octets.append(byteToHexString(m_value[i]));
        }
        octets.append('}');
        return octets.toString();
    }
}
