/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
  String Ids used by mailnews\local.
  To Do: Conver the callers to use names instead of ids and then make this file obsolete.
 */
 
#ifndef _nsLocalStrings_H__
#define _nsLocalStrings_H__

#define LOCAL_STATUS_SELECTING_MAILBOX                        4000
#define LOCAL_STATUS_DOCUMENT_DONE                            4001
#define POP3_SERVER_ERROR                                     4003
#define POP3_USERNAME_FAILURE                                 4004
#define POP3_PASSWORD_FAILURE                                 4005
#define POP3_MESSAGE_WRITE_ERROR                              4006
#define POP3_RETR_FAILURE                                     4012
#define POP3_PASSWORD_UNDEFINED                               4013
#define POP3_USERNAME_UNDEFINED                               4014
#define POP3_LIST_FAILURE                                     4015
#define POP3_DELE_FAILURE                                     4016
#define POP3_STAT_FAILURE                                     4024
#define POP3_SERVER_SAID                                      4025
#define COPYING_MSGS_STATUS                                   4027
#define MOVING_MSGS_STATUS                                    4028
#define POP3_MESSAGE_FOLDER_BUSY                              4029
#define MOVEMAIL_CANT_OPEN_SPOOL_FILE                         4033
#define MOVEMAIL_CANT_CREATE_LOCK                             4034
#define MOVEMAIL_CANT_DELETE_LOCK                             4035
#define MOVEMAIL_CANT_TRUNCATE_SPOOL_FILE                     4036
#define MOVEMAIL_SPOOL_FILE_NOT_FOUND                         4037
#define POP3_TMP_DOWNLOAD_FAILED                              4038
#define POP3_SERVER_DOES_NOT_SUPPORT_UIDL_ETC                 4040
#define POP3_SERVER_DOES_NOT_SUPPORT_THE_TOP_COMMAND          4041
#define NS_ERROR_COULD_NOT_CONNECT_VIA_TLS                    4043
#define POP3_MOVE_FOLDER_TO_TRASH                             4044
#define POP3_DELETE_FOLDER_DIALOG_TITLE                       4045
#define POP3_DELETE_FOLDER_BUTTON_LABEL                       4046
#define POP3_AUTH_INTERNAL_ERROR                              4047
#define POP3_AUTH_CHANGE_ENCRYPT_TO_PLAIN_NO_SSL              4048
#define POP3_AUTH_CHANGE_ENCRYPT_TO_PLAIN_SSL                 4049
#define POP3_AUTH_CHANGE_PLAIN_TO_ENCRYPT                     4050
#define POP3_AUTH_MECH_NOT_SUPPORTED                          4051
#define POP3_GSSAPI_FAILURE                                   4052

#endif /* _nsLocalStrings_H__ */
