/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsMsgI18N_H_
#define _nsMsgI18N_H_

#include "nscore.h"
#include "msgCore.h"
#include "nsStringGlue.h"
class nsIFile;

/**
 * Encode an input string into RFC 2047 form.
 *
 * @param header       [IN] A header to encode.
 * @param structured   [IN] Specify the header is structured or non-structured field (See RFC-822).
 * @param charset      [IN] Charset name to convert.
 * @param fieldnamelen [IN] Header field name length. (e.g. "From: " -> 6)
 * @param usemime      [IN] If false then apply charset conversion only no MIME encoding.
 * @return             Encoded buffer (in C string) or NULL in case of error.
 */
NS_MSG_BASE char      *nsMsgI18NEncodeMimePartIIStr(const char *header, bool structured, const char *charset, int32_t fieldnamelen, bool usemime);

/**
 * Check if given charset is stateful (e.g. ISO-2022-JP).
 *
 * @param charset     [IN] Charset name.
 * @return            True if stateful
 */
NS_MSG_BASE bool      nsMsgI18Nstateful_charset(const char *charset);

/**
 * Check if given charset is multibye (e.g. Shift_JIS, Big5).
 *
 * @param charset     [IN] Charset name.
 * @return            True if multibyte
 */
NS_MSG_BASE bool nsMsgI18Nmultibyte_charset(const char *charset);

/**
 * Check the input (unicode) string is in a range of the given charset after the conversion.
 * Note, do not use this for large string (e.g. message body) since this actually applies the conversion to the buffer.
 *
 * @param charset     [IN] Charset to be converted.
 * @param inString    [IN] Input unicode string to be examined.
 * @param fallbackCharset [OUT]
 *                         null if fallback charset is not needed.
 *                         Otherwise, a fallback charset name may be set if that was used for the conversion. 
 *                         Caller is responsible for freeing the memory.
 * @return            True if the string can be converted within the charset range.
 *                    False if one or more characters cannot be converted to the target charset.
 */
NS_MSG_BASE bool      nsMsgI18Ncheck_data_in_charset_range(const char *charset, const PRUnichar* inString,
                                                           char **fallbackCharset=nullptr);

/**
 * Return charset name of file system (OS dependent).
 *
 * @return            File system charset name.
 */
NS_MSG_BASE const char * nsMsgI18NFileSystemCharset(void);

/**
 * Return charset name of text file (OS dependent).
 *
 * @param aCharset    [OUT] Text file charset name.
 */
NS_MSG_BASE void nsMsgI18NTextFileCharset(nsACString& aCharset);

/**
 * Convert from unicode to target charset.
 *
 * @param charset     [IN] Charset name.
 * @param inString    [IN] Unicode string to convert.
 * @param outString   [OUT] Converted output string.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NConvertFromUnicode(const char* aCharset,
                                                 const nsString& inString,
                                                 nsACString& outString,
                                                 bool aIsCharsetCanonical =
                                                        false);
/**
 * Convert from charset to unicode.
 *
 * @param charset     [IN] Charset name.
 * @param inString    [IN] Input string to convert.
 * @param outString   [OUT] Output unicode string.
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NConvertToUnicode(const char* aCharset,
                                               const nsCString& inString,
                                               nsAString& outString,
                                               bool aIsCharsetCanonical =
                                                      false);
/**
 * Parse for META charset.
 *
 * @param file    [IN] A nsIFile.
 * @return            A charset name or empty string if not found.
 */
NS_MSG_BASE const char *nsMsgI18NParseMetaCharset(nsIFile* file);

/**
 * Convert from charset to unicode. Also does substitution for unconverted characters (e.g. entity, '?').
 *
 * @param contentType [IN] text/plain or text/html.
 * @param charset     [IN] Charset name to convert.
 * @param inString    [IN] Input unicode string to convert.
 * @param outString   [OUT] Allocated and converted output C string. Need PR_FREE.
 * @param fallbackCharset [OUT]
 *                         null if fallback charset is not needed.
 *                         Otherwise, a fallback charset name may be set if that was used for the conversion. 
 *                         Caller is responsible for freeing the memory.
 * @param isAsciiOnly [OUT]
 *                         null if non ASCII info is not needed.
 *                         Otherwise, true is set if the input data is ASCII only false otherwise. 
 * @return            nsresult.
 */
NS_MSG_BASE nsresult nsMsgI18NSaveAsCharset(const char* contentType, const char* charset, 
                                            const PRUnichar* inString, char** outString, 
                                            char **fallbackCharset=nullptr, bool *isAsciiOnly=nullptr);

/**
 * Shrink the aStr to aMaxLength bytes. Note that this doesn't check whether
 * the aUTF8Str is valid UTF-8 string.
 *
 * @param inString   [IN] Input UTF-8 string (it must be valid UTF-8 string)
 * @param aMaxLength [IN] Shrink to this length (it means bytes)
 * @param outString  [OUT] Shrunken UTF-8 string
 * @return           nsresult
 */
NS_MSG_BASE nsresult nsMsgI18NShrinkUTF8Str(const nsCString &inString,
                                            uint32_t aMaxLength,
                                            nsACString &outString);

/*
 * Convert raw bytes in header to UTF-16
 *
 * @param inString   [IN] Input raw octets
 * @param outString  [OUT] Output UTF-16 string
 */
NS_MSG_BASE void nsMsgI18NConvertRawBytesToUTF16(const nsCString& inString, 
                                                 const char* charset,
                                                 nsAString& outString);

/*
 * Convert raw bytes in header to UTF-8
 *
 * @param inString   [IN] Input raw octets
 * @param outString  [OUT] Output UTF-8 string
 */
NS_MSG_BASE void nsMsgI18NConvertRawBytesToUTF8(const nsCString& inString, 
                                                const char* charset,
                                                nsACString& outString);

// inline forwarders to avoid littering with 'x-imap4-.....'
inline nsresult CopyUTF16toMUTF7(const nsString &aSrc, nsACString& aDest)
{
    return nsMsgI18NConvertFromUnicode("x-imap4-modified-utf7", aSrc,
                                       aDest, true);
}

inline nsresult CopyMUTF7toUTF16(const nsCString& aSrc, nsAString& aDest)
{
    return nsMsgI18NConvertToUnicode("x-imap4-modified-utf7", aSrc,
                                     aDest, true);
}

inline nsresult ConvertToUnicode(const char* charset,
                                 const nsCString &aSrc, nsAString& aDest)
{
    return nsMsgI18NConvertToUnicode(charset, aSrc, aDest);
}

inline nsresult ConvertToUnicode(const char* charset,
                                 const char* aSrc, nsAString& aDest)
{
    return nsMsgI18NConvertToUnicode(charset, nsDependentCString(aSrc), aDest);
}

inline nsresult ConvertFromUnicode(const char* charset,
                                   const nsString &aSrc, nsACString& aDest)
{
    return nsMsgI18NConvertFromUnicode(charset, aSrc, aDest);
}

inline void ConvertRawBytesToUTF16(const nsCString& inString, 
                                   const char* charset, nsAString& outString)
{
    return nsMsgI18NConvertRawBytesToUTF16(inString, charset, outString);
}

inline void ConvertRawBytesToUTF16(const char* inString, 
                                   const char* charset, nsAString& outString)
{
    return nsMsgI18NConvertRawBytesToUTF16(nsDependentCString(inString),
                                           charset,
                                           outString);
}

inline void ConvertRawBytesToUTF8(const nsCString& inString, 
                                  const char* charset, nsACString& outString)
{
    return nsMsgI18NConvertRawBytesToUTF8(inString, charset, outString);
}

#endif /* _nsMsgI18N_H_ */
