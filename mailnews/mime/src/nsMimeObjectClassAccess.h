/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This interface is implemented by libmime. This interface is used by
 * a Content-Type handler "Plug In" (i.e. vCard) for accessing various
 * internal information about the object class system of libmime. When
 * libmime progresses to a C++ object class, this would probably change.
 */
#ifndef nsMimeObjectClassAccess_h_
#define nsMimeObjectClassAccess_h_

#include "nsISupports.h"
#include "prtypes.h"
#include "nsIMimeObjectClassAccess.h"

class nsMimeObjectClassAccess : public nsIMimeObjectClassAccess {
public:
  nsMimeObjectClassAccess();
  virtual ~nsMimeObjectClassAccess();

  /* this macro defines QueryInterface, AddRef and Release for this class */
  NS_DECL_ISUPPORTS

  // These methods are all implemented by libmime to be used by
  // content type handler plugins for processing stream data.

  // This is the write call for outputting processed stream data.
  NS_IMETHOD    MimeObjectWrite(void *mimeObject,
                                char *data,
                                int32_t length,
                                bool user_visible_p);

  // The following group of calls expose the pointers for the object
  // system within libmime.
  NS_IMETHOD    GetmimeInlineTextClass(void **ptr);
  NS_IMETHOD    GetmimeLeafClass(void **ptr);
  NS_IMETHOD    GetmimeObjectClass(void **ptr);
  NS_IMETHOD    GetmimeContainerClass(void **ptr);
  NS_IMETHOD    GetmimeMultipartClass(void **ptr);
  NS_IMETHOD    GetmimeMultipartSignedClass(void **ptr);
  NS_IMETHOD    GetmimeEncryptedClass(void **ptr);

  NS_IMETHOD    MimeCreate(char *content_type, void * hdrs, void * opts, void**ptr);
};

#endif /* nsMimeObjectClassAccess_h_ */
