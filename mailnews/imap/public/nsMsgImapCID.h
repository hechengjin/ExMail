/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMsgImapCID_h__
#define nsMsgImapCID_h__

#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIComponentManager.h"
#include "nsMsgBaseCID.h"

#define NS_IMAPURL_CID							\
{ /* 21A89611-DC0D-11d2-806C-006008128C4E */	\
  0x21a89611, 0xdc0d, 0x11d2,					\
   { 0x80, 0x6c, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e }}

#define NS_IMAPPROTOCOLINFO_CONTRACTID \
  NS_MSGPROTOCOLINFO_CONTRACTID_PREFIX "imap"

#define NS_IMAPINCOMINGSERVER_CONTRACTID  \
  NS_MSGINCOMINGSERVER_CONTRACTID_PREFIX "imap"

#define NS_IMAPSERVICE_CONTRACTID \
  "@mozilla.org/messenger/imapservice;1"

#define NS_IMAPSERVICE_CID						  \
{ /* C5852B22-EBE2-11d2-95AD-000064657374 */      \
 0xc5852b22, 0xebe2, 0x11d2,                      \
 {0x95, 0xad, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74}}

#define NS_IMAPPROTOCOL_CID							\
{ /* 8C0C40D1-E173-11d2-806E-006008128C4E */		\
  0x8c0c40d1, 0xe173, 0x11d2,						\
    { 0x80, 0x6e, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e }}


#define NS_IIMAPHOSTSESSIONLIST_CID					\
{ /* 479ce8fc-e725-11d2-a505-0060b0fc04b7 */		\
  0x479ce8fc, 0xe725, 0x11d2,						\
	{0xa5, 0x05, 0x00, 0x60, 0xb0, 0xfc, 0x04, 0xb7 }}

#define NS_IMAPINCOMINGSERVER_CID				  \
{ /* 8D3675E0-ED46-11d2-8077-006008128C4E */      \
 0x8d3675e0, 0xed46, 0x11d2,	                  \
 {0x80, 0x77, 0x0, 0x60, 0x8, 0x12, 0x8c, 0x4e}}

#define NS_IMAPRESOURCE_CID						\
{ /* fa32d000-f6a0-11d2-af8d-001083002da8 */	\
  0xfa32d000, 0xf6a0, 0x11d2,					\
  { 0xaf, 0x8d, 0x00, 0x10, 0x83, 0x00, 0x2d, 0xa8 }}

// 4ECA51DF-6734-11d3-989A-001083010E9B
#define NS_IMAPMOCKCHANNEL_CID \
	{0x4eca51df, 0x6734, 0x11d3, \
	{0x98, 0x9a, 0x0, 0x10, 0x83, 0x1, 0xe, 0x9b}}

#endif // nsMsgImapCID_h__
