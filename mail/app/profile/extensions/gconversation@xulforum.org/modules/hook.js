var EXPORTED_SYMBOLS = ['registerHook', 'getHooks', 'removeHook'];

/* A hook is just a listener whose various methods get called at various stages
 *  of the conversation process.
 *
 * let hook = {
 *  // Called before the given message is streamed into the <iframe>.
 *  onMessageBeforeStreaming (aMessage) {
 *  },
 *
 *  // Called when the given message has been displayed.
 *  onMessageStreamed (aMsgHdr, aDomNode, aMsgWindow, aMessage) {
 *  },
 *
 *  // Called before the quick reply message is send or opened in a new
 *  // compose-window.
 *  // Set of functions with different priorities, call to "_early" function
 *  // should not expect parameters (like sender, recipients) to keep stable.
 *  // "_canceled"-function will only be called if sending was canceled - you
 *  // should not expect to change the returned sending-status here.
 *  // @param aAddress.param The params (sender, identity) the user picked
 *  //  to send the message.
 *  // @param aAddress.to The recipients. This is an Array of valid email
 *  //  addresses.
 *  // @param aAddress.cc Same remark.
 *  // @param aAddress.bcc Same remark.
 *  // @param aEditor The textarea node which value is mail body.
 *  // @param aStatus.canceled Sending the message is canceled.
 *  // @param aStatus.securityInfo An object for PGM/MIME message.
 *  // @param aPopout if set, message will not be opened in compose window
 *  // @param aAttachmentList The AttachmentList object.
 *  // @return aStatus Same remark.
 *  onMessageBeforeSendOrPopout_early: function (aAddress, aEditor, aStatus, aPopout, aAttachmentList) {
 *  },
 *  onMessageBeforeSendOrPopout: function (aAddress, aEditor, aStatus, aPopout, aAttachmentList) {
 *  },
 *  onMessageBeforeSendOrPopout_canceled: function (aAddress, aEditor, aStatus, aPopout, aAttachmentList) {
 *  },
 *
 *  // Called when quick reply body is composed.
 *  // @param aMessage Original message instance.
 *  // @param aBody Quoted body of original message.
 *  // @return aBody Same remark.
 *  onReplyComposed (aMessage, aBody) {
 *  },
 *
 *  // Called when a message is selected.
 *  // @param aMessage Selected message instance.
 *  onMessageSelected (aMessage) {
 *  },
 *
 *  // Called whenever a new quickreply Compose-Session was
 *  // created and all fields are prepared
 *  // @param aComposeSession created composeSession
 *  // @param aMessage current message instance
 *  // @param aAddress.to The recipients. This is an Array of valid email
 *  //  addresses.
 *  // @param aAddress.cc Same remark.
 *  // @param aAddress.bcc Same remark.
 *  onComposeSessionChanged (aComposeSession, aMessage, aAddress) {
 *  },
 *
 *  // Called whenever a recipient is added to the quickreply
 *  // @param aData list data containing user input
 *  // @param aType type of changed recipient (to, cc, bcc)
 *  // @param aCount number of recipients of this type
 *  onRecipientAdded(aData, aType, aCount) {
 *  },
 *
 *  // Called regardless of whether the Sending operation was successful.
 *  // @param aMsgID   The message id for the mail message
 *  // @param aStatus   Status code for the message send.
 *  // @param aMsg      A text string describing the error.
 *  // @param aReturnFile The returned file spec for save to file operations.
 *  onStopSending (aMsgID, aStatus, aMsg, aReturnFile) {
 *  }
 * }
 *
 * If you need something else, just ask!
 * */
let hooks = [];

function registerHook(h) {
  hooks.push(h);
}

function removeHook(h) {
  hooks = hooks.filter(function (x) x != h);
}

function getHooks() {
  return hooks;
}
