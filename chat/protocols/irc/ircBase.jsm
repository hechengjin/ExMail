/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This contains the implementation for the basic Internet Relay Chat (IRC)
 * protocol covered by RFCs 2810, 2811, 2812 and 2813 (which obsoletes RFC
 * 1459). RFC 2812 covers the client commands and protocol.
 *   RFC 2810: Internet Relay Chat: Architecture
 *     http://tools.ietf.org/html/rfc2810
 *   RFC 2811: Internet Relay Chat: Channel Management
 *     http://tools.ietf.org/html/rfc2811
 *   RFC 2812: Internet Relay Chat: Client Protocol
 *     http://tools.ietf.org/html/rfc2812
 *   RFC 2813: Internet Relay Chat: Server Protocol
 *     http://tools.ietf.org/html/rfc2813
 *   RFC 1459: Internet Relay Chat Protocol
 *     http://tools.ietf.org/html/rfc1459
 */
const EXPORTED_SYMBOLS = ["ircBase"];

const {interfaces: Ci, utils: Cu} = Components;

Cu.import("resource:///modules/imXPCOMUtils.jsm");
Cu.import("resource:///modules/imServices.jsm");
Cu.import("resource:///modules/ircHandlers.jsm");
Cu.import("resource:///modules/ircUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "DownloadUtils", function() {
  Components.utils.import("resource://gre/modules/DownloadUtils.jsm");
  return DownloadUtils;
});

function privmsg(aAccount, aMessage, aIsNotification) {
  let params = {incoming: true};
  if (aIsNotification)
    params.notification = true;
  aAccount.getConversation(aAccount.isMUCName(aMessage.params[0]) ?
                           aMessage.params[0] : aMessage.nickname)
          .writeMessage(aMessage.nickname || aMessage.servername,
                        aMessage.params[1], params);
  return true;
}

// Display the message and remove them from the rooms they're in.
function leftRoom(aAccount, aNicks, aChannels, aSource, aReason, aKicked) {
  let msgId = "message." +  (aKicked ? "kicked" : "parted");
  // If a part message was included, include it.
  let reason = aReason ? _(msgId + ".reason", aReason) : "";
  function __(aNick, aYou) {
    // If the user is kicked, we need to say who kicked them.
    let msgId2 = msgId + (aYou ? ".you" : "");
    if (aKicked) {
      if (aYou)
        return _(msgId2, aSource, reason);
      return _(msgId2, aNick, aSource, reason);
    }
    if (aYou)
      return _(msgId2, reason);
    return _(msgId2, aNick, reason);
  }

  for each (let channelName in aChannels) {
    if (!aAccount.hasConversation(channelName))
      continue; // Handle when we closed the window
    let conversation = aAccount.getConversation(channelName);
    for each (let nick in aNicks) {
      let msg;
      if (aAccount.normalize(nick) == aAccount.normalize(aAccount._nickname)) {
        msg = __(nick, true);
        // If the user left, mark the conversation as no longer being active.
        conversation.left = true;
        conversation.notifyObservers(conversation, "update-conv-chatleft");
      }
      else
        msg = __(nick);

      conversation.writeMessage(aSource, msg, {system: true});
      conversation.removeParticipant(nick, true);
    }
  }
  return true;
}

function writeMessage(aAccount, aMessage, aString, aType) {
  let type = {};
  type[aType] = true;
  aAccount.getConversation(aMessage.servername)
          .writeMessage(aMessage.servername, aString, type);
  return true;
}

// If aNoLastParam is true, the last parameter is not printed out.
function serverMessage(aAccount, aMsg, aNoLastParam) {
  // If we don't want to show messages from the server, just mark it as handled.
  if (!aAccount._showServerTab)
    return true;

  return writeMessage(aAccount, aMsg,
                      aMsg.params.slice(1, aNoLastParam ? -1 : undefined).join(" "),
                      "system");
}

function serverErrorMessage(aAccount, aMessage, aError) {
  // If we don't want to show messages from the server, just mark it as handled.
  if (!aAccount._showServerTab)
    return true;

  return writeMessage(aAccount, aMessage, aError, "error")
}

function conversationErrorMessage(aAccount, aMessage, aError) {
  let conv = aAccount.getConversation(aMessage.params[1]);
  conv.writeMessage(aMessage.servername, _(aError, aMessage.params[1]),
                    {error: true, system: true});
  delete conv._pendingMessage;
  return true;
}

// Try a new nick if the previous tried nick is already in use.
function tryNewNick(aAccount, aMessage) {
  let nickParts = /^(.+?)(\d*)$/.exec(aMessage.params[1]);
  let newNick = nickParts[1];

  // If there was not a digit at the end of the nick, just append 1.
  let newDigits = "1";
  // If there was a digit at the end of the nick, increment it.
  if (nickParts[2]) {
    newDigits = (parseInt(nickParts[2], 10) + 1).toString();
    // If there were leading 0s, add them back on, after we've incremented (e.g.
    // 009 --> 010).
    for (let len = nickParts[2].length - newDigits.length; len > 0; --len)
      newDigits = "0" + newDigits;
  }
  // If the nick will be too long, ensure all the digits fit.
  if (newNick.length + newDigits.length > aAccount.maxNicknameLength) {
    // Handle the silly case of a single letter followed by all nines.
    if (newDigits.length == aAccount.maxNicknameLength)
      newDigits = newDigits.slice(1);
    newNick = newNick.slice(0, aAccount.maxNicknameLength - newDigits.length);
  }
  // Append the digits.
  newNick += newDigits;

  LOG(aMessage.params[1] + " is already in use, trying " + newNick);
  aAccount.sendMessage("NICK", newNick); // Nick message.
  return true;
}

// See RFCs 2811 & 2812 (which obsoletes RFC 1459) for a description of these
// commands.
var ircBase = {
  // Parameters
  name: "RFC 2812", // Name identifier
  priority: ircHandlers.DEFAULT_PRIORITY,
  isEnabled: function() true,

  // The IRC commands that can be handled.
  commands: {
    "ERROR": function(aMessage) {
      // ERROR <error message>
      // Client connection has been terminated.
      if (!this.disconnecting) {
        // We received an ERROR message when we weren't expecting it, this is
        // probably the server giving us a ping timeout.
        ERROR("Received unexpected ERROR response:\n" + aMessage.params[0]);
        this.gotDisconnected(Ci.prplIAccount.ERROR_NETWORK_ERROR,
                             _("connection.error.lost"));
      }
      else {
        // We received an ERROR message when expecting it (i.e. we've sent a
        // QUIT command).
        clearTimeout(this._quitTimer);
        delete this._quitTimer;
        // Notify account manager.
        this.gotDisconnected();
      }
      return true;
    },
    "INVITE": function(aMessage) {
      // INVITE <nickname> <channel>
      // Auto-accept the invite.
      this.joinChat(this.getChatRoomDefaultFieldValues(aMessage.params[1]));
      this.getConversation(aMessage.params[1])
          .writeMessage(aMessage.nickname,
                        _("message.inviteReceived", aMessage.nickname,
                          aMessage.params[1]), {system: true});
      return true;
    },
    "JOIN": function(aMessage) {
      // JOIN ( <channel> *( "," <channel> ) [ <key> *( "," <key> ) ] ) / "0"
      // Add the buddy to each channel
      for each (let channelName in aMessage.params[0].split(",")) {
        let conversation = this.getConversation(channelName);
        if (this.normalize(aMessage.nickname, this.userPrefixes) ==
            this.normalize(this._nickname)) {
          // If you join, clear the participants list to avoid errors w/
          // repeated participants.
          conversation.removeAllParticipants();
          conversation.left = false;
          conversation.notifyObservers(conversation, "update-conv-chatleft");
          delete this._chatRoomFieldsList[this.normalize(conversation.name)];
        }
        else {
          // Don't worry about adding ourself, RPL_NAMES takes care of that
          // case.
          conversation.getParticipant(aMessage.nickname, true);
          let msg = _("message.join", aMessage.nickname, aMessage.source || "");
          conversation.writeMessage(aMessage.nickname, msg, {system: true,
                                                             noLinkification: true});
        }
      }
      // If the joiner is a buddy, mark as online.
      let buddy = this.getBuddy(aMessage.nickname);
      if (buddy)
        buddy.setStatus(Ci.imIStatusInfo.STATUS_AVAILABLE, "");
      return true;
    },
    "KICK": function(aMessage) {
      // KICK <channel> *( "," <channel> ) <user> *( "," <user> ) [<comment>]
      let comment = aMessage.params.length == 3 ? aMessage.params[2] : null;
      // Some servers (moznet) send the kicker as the comment.
      if (comment == aMessage.nickname)
        comment = null;
      return leftRoom(this, aMessage.params[1].split(","),
                      aMessage.params[0].split(","), aMessage.nickname,
                      comment, true);
    },
    "MODE": function(aMessage) {
      // MODE <nickname> *( ( "+" / "-") *( "i" / "w" / "o" / "O" / "r" ) )
      // MODE <channel> *( ( "-" / "+" ) *<modes> *<modeparams> )
      if (aMessage.params.length >= 3) {
        // If there are 3 parameters given, then the mode of a participant is
        // being given: update the mode of the ConvChatBuddy.
        let conversation = this.getConversation(aMessage.params[0]);
        conversation.getParticipant(aMessage.params[2])
                    .setMode(aMessage.params[1], aMessage.nickname);

        return true;
      }
      if (this.isMUCName(aMessage.params[0])) {
        // Otherwise if the first parameter is a channel name, it's a channel
        // mode.
        this.getConversation(aMessage.params[0])
            .setMode(aMessage.params[1], aMessage);

        return true;
      }
      // Otherwise the user's own mode is being returned to them.
      // TODO
      return false;
    },
    "NICK": function(aMessage) {
      // NICK <nickname>
      this.changeBuddyNick(aMessage.nickname, aMessage.params[0]);
      return true;
    },
    "NOTICE": function(aMessage) {
      // NOTICE <msgtarget> <text>
      // If the message doesn't have a nickname, it's from the server, don't
      // show it unless the user wants to see it.
      if (!aMessage.hasOwnProperty("nickname"))
        return serverMessage(this, aMessage);
      return privmsg(this, aMessage, true);
    },
    "PART": function(aMessage) {
      // PART <channel> *( "," <channel> ) [ <Part Message> ]
      return leftRoom(this, [aMessage.nickname], aMessage.params[0].split(","),
                      aMessage.source || "",
                      aMessage.params.length == 2 ? aMessage.params[1] : null);
    },
    "PING": function(aMessage) {
      // PING <server1 [ <server2> ]
      // Keep the connection alive.
      this.sendMessage("PONG", aMessage.params[0]);
      return true;
    },
    "PRIVMSG": function(aMessage) {
      // PRIVMSG <msgtarget> <text to be sent>
      // Display message in conversation
      return privmsg(this, aMessage);
    },
    "QUIT": function(aMessage) {
      // QUIT [ < Quit Message> ]
      // Some IRC servers automatically prefix a "Quit: " string. Remove the
      // duplication and use a localized version.
      let quitMsg = aMessage.params[0] || "";
      if (quitMsg.indexOf("Quit: ") == 0)
        quitMsg = quitMsg.slice(6); // "Quit: ".length
      // If a quit message was included, show it.
      let msg = _("message.quit", aMessage.nickname,
                  quitMsg.length ? _("message.quit2", quitMsg) : "");
      // Loop over every conversation with the user and display that they quit.
      for each (let conversation in this._conversations) {
        if (conversation.isChat &&
            conversation.hasParticipant(aMessage.nickname)) {
          conversation.writeMessage(aMessage.servername, msg, {system: true});
          conversation.removeParticipant(aMessage.nickname, true);
        }
      }

      // Remove from the whois table.
      this.removeBuddyInfo(aMessage.nickname);

      // If the leaver is a buddy, mark as offline.
      let buddy = this.getBuddy(aMessage.nickname);
      if (buddy)
        buddy.setStatus(Ci.imIStatusInfo.STATUS_OFFLINE, "");
      return true;
    },
    "SQUIT": function(aMessage) {
      // <server> <comment>
      return true;
    },
    "TOPIC": function(aMessage) {
      // TOPIC <channel> [ <topic> ]
      // Show topic as a message.
      let source = aMessage.nickname || aMessage.servername;
      let conversation = this.getConversation(aMessage.params[0]);
      let topic = aMessage.params[1];
      // Set the topic in the conversation and update the UI.
      conversation.setTopic(topic ? ctcpFormatToText(topic) : "", source);
      return true;
    },
    "001": function(aMessage) { // RPL_WELCOME
      // Welcome to the Internet Relay Network <nick>!<user>@<host>
      this.reportConnected();
      // Check if our nick has changed.
      if (aMessage.params[0] != this._nickname)
        this.changeBuddyNick(this._nickname, aMessage.params[0]);
      // If our status is Unavailable, tell the server.
      if (this.imAccount.statusInfo.statusType < Ci.imIStatusInfo.STATUS_AVAILABLE)
        this.observe(null, "status-changed");
      // Check if any of our buddies are online!
      this.sendIsOn();
      // Reconnect channels if they were not parted by the user.
      for each (let conversation in this._conversations) {
        if (conversation.isChat && conversation._chatRoomFields)
          this.joinChat(conversation._chatRoomFields);
      }
      return serverMessage(this, aMessage);
    },
    "002": function(aMessage) { // RPL_YOURHOST
      // Your host is <servername>, running version <ver>
      return serverMessage(this, aMessage);
    },
    "003": function(aMessage) { // RPL_CREATED
      // This server was created <date>
      // TODO parse this date and keep it for some reason? Do we care?
      return serverMessage(this, aMessage);
    },
    "004": function(aMessage) { // RPL_MYINFO
      // <servername> <version> <available user modes> <available channel modes>
      // TODO parse the available modes, let the UI respond and inform the user
      return serverMessage(this, aMessage);
    },
    "005": function(aMessage) { // RPL_BOUNCE
      // Try server <server name>, port <port number>
      return serverMessage(this, aMessage);
    },

    /*
     * Handle response to TRACE message
     */
    "200": function(aMessage) { // RPL_TRACELINK
      // Link <version & debug level> <destination> <next server>
      // V<protocol version> <link updateime in seconds> <backstream sendq>
      // <upstream sendq>
      return serverMessage(this, aMessage);
    },
    "201": function(aMessage) { // RPL_TRACECONNECTING
      // Try. <class> <server>
      return serverMessage(this, aMessage);
    },
    "202": function(aMessage) { // RPL_TRACEHANDSHAKE
      // H.S. <class> <server>
      return serverMessage(this, aMessage);
    },
    "203": function(aMessage) { // RPL_TRACEUNKNOWN
      // ???? <class> [<client IP address in dot form>]
      return serverMessage(this, aMessage);
    },
    "204": function(aMessage) { // RPL_TRACEOPERATOR
      // Oper <class> <nick>
      return serverMessage(this, aMessage);
    },
    "205": function(aMessage) { // RPL_TRACEUSER
      // User <class> <nick>
      return serverMessage(this, aMessage);
    },
    "206": function(aMessage) { // RPL_TRACESERVER
      // Serv <class> <int>S <int>C <server> <nick!user|*!*>@<host|server>
      // V<protocol version>
      return serverMessage(this, aMessage);
    },
    "207": function(aMessage) { // RPL_TRACESERVICE
      // Service <class> <name> <type> <active type>
      return serverMessage(this, aMessage);
    },
    "208": function(aMessage) { // RPL_TRACENEWTYPE
      // <newtype> 0 <client name>
      return serverMessage(this, aMessage);
    },
    "209": function(aMessage) { // RPL_TRACECLASS
      // Class <class> <count>
      return serverMessage(this, aMessage);
    },
    "210": function(aMessage) { // RPL_TRACERECONNECTION
      // Unused.
      return serverMessage(this, aMessage);
    },

    /*
     * Handle stats messages.
     **/
    "211": function(aMessage) { // RPL_STATSLINKINFO
      // <linkname> <sendq> <sent messages> <sent Kbytes> <received messages>
      // <received Kbytes> <time open>
      return serverMessage(this, aMessage);
    },
    "212": function(aMessage) { // RPL_STATSCOMMAND
      // <command> <count> <byte count> <remote count>
      return serverMessage(this, aMessage);
    },
    "213": function(aMessage) { // RPL_STATSCLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "214": function(aMessage) { // RPL_STATSNLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "215": function(aMessage) { // RPL_STATSILINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "216": function(aMessage) { // RPL_STATSKLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "217": function(aMessage) { // RPL_STATSQLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "218": function(aMessage) { // RPL_STATSYLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "219": function(aMessage) { // RPL_ENDOFSTATS
      // <stats letter> :End of STATS report
      return serverMessage(this, aMessage);
    },

    "221": function(aMessage) { // RPL_UMODEIS
      // <user mode string>
      // TODO track and update the UI accordingly.
      return false;
    },

    /*
     * Services
     */
    "231": function(aMessage) { // RPL_SERVICEINFO
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "232": function(aMessage) { // RPL_ENDOFSERVICES
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "233": function(aMessage) { // RPL_SERVICE
      // Non-generic
      return serverMessage(this, aMessage);
    },

    /*
     * Server
     */
    "234": function(aMessage) { // RPL_SERVLIST
      // <name> <server> <mask> <type> <hopcount> <info>
      return serverMessage(this, aMessage);
    },
    "235": function(aMessage) { // RPL_SERVLISTEND
      // <mask> <type> :End of service listing
      return serverMessage(this, aMessage, true);
    },

    /*
     * Stats
     * TODO some of these have real information we could try to parse.
     */
    "240": function(aMessage) { // RPL_STATSVLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "241": function(aMessage) { // RPL_STATSLLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "242": function(aMessage) { // RPL_STATSUPTIME
      // :Server Up %d days %d:%02d:%02d
      return serverMessage(this, aMessage);
    },
    "243": function(aMessage) { // RPL_STATSOLINE
      // O <hostmask> * <name>
      return serverMessage(this, aMessage);
    },
    "244": function(aMessage) { // RPL_STATSHLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "245": function(aMessage) { // RPL_STATSSLINE
      // Non-generic
      // Note that this is given as 244 in RFC 2812, this seems to be incorrect.
      return serverMessage(this, aMessage);
    },
    "246": function(aMessage) { // RPL_STATSPING
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "247": function(aMessage) { // RPL_STATSBLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },
    "250": function(aMessage) { // RPL_STATSDLINE
      // Non-generic
      return serverMessage(this, aMessage);
    },

    /*
     * LUSER messages
     */
    "251": function(aMessage) { // RPL_LUSERCLIENT
      // :There are <integer> users and <integer> services on <integer> servers
      return serverMessage(this, aMessage);
    },
    "252": function(aMessage) { // RPL_LUSEROP, 0 if not sent
      // <integer> :operator(s) online
      return serverMessage(this, aMessage);
    },
    "253": function(aMessage) { // RPL_LUSERUNKNOWN, 0 if not sent
      // <integer> :unknown connection(s)
      return serverMessage(this, aMessage);
    },
    "254": function(aMessage) { // RPL_LUSERCHANNELS, 0 if not sent
      // <integer> :channels formed
      return serverMessage(this, aMessage);
    },
    "255": function(aMessage) { // RPL_LUSERME
      // :I have <integer> clients and <integer> servers
      return serverMessage(this, aMessage);
    },

    /*
     * ADMIN messages
     */
    "256": function(aMessage) { // RPL_ADMINME
      // <server> :Administrative info
      return serverMessage(this, aMessage);
    },
    "257": function(aMessage) { // RPL_ADMINLOC1
      // :<admin info>
      // City, state & country
      return serverMessage(this, aMessage);
    },
    "258": function(aMessage) { // RPL_ADMINLOC2
      // :<admin info>
      // Institution details
      return serverMessage(this, aMessage);
    },
    "259": function(aMessage) { // RPL_ADMINEMAIL
      // :<admin info>
      // TODO We could parse this for a contact email.
      return serverMessage(this, aMessage);
    },

    /*
     * TRACELOG
     */
    "261": function(aMessage) { // RPL_TRACELOG
      // File <logfile> <debug level>
      return serverMessage(this, aMessage);
    },
    "262": function(aMessage) { // RPL_TRACEEND
      // <server name> <version & debug level> :End of TRACE
      return serverMessage(this, aMessage, true);
    },

    /*
     * Try again.
     */
    "263": function(aMessage) { // RPL_TRYAGAIN
      // <command> :Please wait a while and try again.
      // TODO setTimeout for a minute or so and try again?
      return false;
    },

    "265": function(aMessage) { // nonstandard
      // :Current Local Users: <integer>  Max: <integer>
      return serverMessage(this, aMessage);
    },
    "266": function(aMessage) { // nonstandard
      // :Current Global Users: <integer>  Max: <integer>
      return serverMessage(this, aMessage);
    },
    "300": function(aMessage) { // RPL_NONE
      // Non-generic
      return serverMessage(this, aMessage);
    },

    /*
     * Status messages
     */
    "301": function(aMessage) { // RPL_AWAY
      // <nick> :<away message>
      // TODO set user as away on buddy list / conversation lists
      // TODO Display an autoResponse if this is after sending a private message
      // If the conversation is waiting for a response, it's received one.
      if (this.hasConversation(aMessage.params[1]))
        delete this.getConversation(aMessage.params[1])._pendingMessage;
      return this.setWhois(aMessage.params[1], {away: aMessage.params[2]});
    },
    "302": function(aMessage) { // RPL_USERHOST
      // :*1<reply> *( " " <reply )"
      // reply = nickname [ "*" ] "=" ( "+" / "-" ) hostname
      // TODO Can tell op / away from this
      return false;
    },
    "303": function(aMessage) { // RPL_ISON
      // :*1<nick> *( " " <nick> )"
      // Set the status of the buddies based the lastest ISON response.
      let receivedBuddyNames = [];
      // The buddy names as returned by the server.
      if (aMessage.params.length > 1)
        receivedBuddyNames = aMessage.params[1].trim().split(" ");

      // This was received in response to the last ISON message sent.
      for each (let buddyName in this.pendingIsOnQueue) {
        // If the buddy name is in the list returned from the server, they're
        // online.
        let status = (receivedBuddyNames.indexOf(buddyName) == -1) ?
                       Ci.imIStatusInfo.STATUS_OFFLINE :
                       Ci.imIStatusInfo.STATUS_AVAILABLE;

        // Set the status with no status message, only if the buddy actually
        // exists in the buddy list.
        let buddy = this.getBuddy(buddyName);
        if (buddy)
          buddy.setStatus(status, "");
      }
      return true;
    },
    "305": function(aMessage) { // RPL_UNAWAY
      // :You are no longer marked as being away
      this.isAway = false;
      return true;
    },
    "306": function(aMessage) { // RPL_NOWAWAY
      // :You have been marked as away
      this.isAway = true;
      return true;
    },

    /*
     * WHOIS
     */
    "311": function(aMessage) { // RPL_WHOISUSER
      // <nick> <user> <host> * :<real name>
      // <username>@<hostname>
      let source = aMessage.params[2] + "@" + aMessage.params[3];
      return this.setWhois(aMessage.params[1], {realname: aMessage.params[5],
                                                connectedFrom: source});
    },
    "312": function(aMessage) { // RPL_WHOISSERVER
      // <nick> <server> :<server info>
      return this.setWhois(aMessage.params[1],
                           {serverName: aMessage.params[2],
                            serverInfo: aMessage.params[3]});
    },
    "313": function(aMessage) { // RPL_WHOISOPERATOR
      // <nick> :is an IRC operator
      return this.setWhois(aMessage.params[1], {ircOp: true});
    },
    "314": function(aMessage) { // RPL_WHOWASUSER
      // <nick> <user> <host> * :<real name>
      let source = aMessage.params[2] + "@" + aMessage.params[3];
      return this.setWhois(aMessage.params[1], {offline: true,
                                                realname: aMessage.params[5],
                                                connectedFrom: source});
    },
    "315": function(aMessage) { // RPL_ENDOFWHO
      // <name> :End of WHO list
      return false;
    },
    "316": function(aMessage) { // RPL_WHOISCHANOP
      // Non-generic
      return false;
    },
    "317": function(aMessage) { // RPL_WHOISIDLE
      // <nick> <integer> :seconds idle
      let valuesAndUnits =
        DownloadUtils.convertTimeUnits(parseInt(aMessage.params[2]));
      if (!valuesAndUnits[2])
        valuesAndUnits.splice(2, 2);
      return this.setWhois(aMessage.params[1],
                           {idleTime: valuesAndUnits.join(" ")});
    },
    "318": function(aMessage) { // RPL_ENDOFWHOIS
      // <nick> :End of WHOIS list
      // We've received everything about WHOIS, tell the tooltip that is waiting
      // for this information.
      let nick = this.normalize(aMessage.params[1]);

      if (hasOwnProperty(this.whoisInformation, nick)) {
        Services.obs.notifyObservers(this.getBuddyInfo(nick),
                                     "user-info-received", nick);
      }
      else {
        // If there is no whois information stored at this point, the nick
        // is either offline or does not exist, so we run WHOWAS.
        this.requestOfflineBuddyInfo(nick);
      }
      return true;
    },
    "319": function(aMessage) { // RPL_WHOISCHANNELS
      // <nick> :*( ( "@" / "+" ) <channel> " " )
      return this.setWhois(aMessage.params[1], {channels: aMessage.params[2]});
    },

    /*
     * LIST
     */
    "321": function(aMessage) { // RPL_LISTSTART
      // Obsolete. Not used.
      return false;
    },
    "322": function(aMessage) { // RPL_LIST
      // <channel> <# visible> :<topic>
      // TODO parse this for # users & topic.
      return serverMessage(this, aMessage);
    },
    "323": function(aMessage) { // RPL_LISTEND
      // :End of LIST
      return true;
    },

    /*
     * Channel functions
     */
    "324": function(aMessage) { // RPL_CHANNELMODEIS
      // <channel> <mode> <mode params>
      this.getConversation(aMessage.params[1]).setMode(aMessage.params[2],
                                                       aMessage);

      return true;
    },
    "325": function(aMessage) { // RPL_UNIQOPIS
      // <channel> <nickname>
      // TODO parse this and have the UI respond accordingly.
      return false;
    },
    "331": function(aMessage) { // RPL_NOTOPIC
      // <channel> :No topic is set
      let conversation = this.getConversation(aMessage.params[1]);
       // Clear the topic.
      conversation.setTopic("");
      return true;
    },
    "332": function(aMessage) { // RPL_TOPIC
      // <channel> :<topic>
      // Update the topic UI
      let conversation = this.getConversation(aMessage.params[1]);
      let topic = aMessage.params[2];
      conversation.setTopic(topic ? ctcpFormatToText(topic) : "");
      return true;
    },
    "333": function(aMessage) { // nonstandard
      // <channel> <nickname> <time>
      return true;
    },

    /*
     * Invitations
     */
    "341": function(aMessage) { // RPL_INVITING
      // <channel> <nick>
      // Note that servers reply with parameters in the reverse order from the
      // above (which is as specified by RFC 2812).
      this.getConversation(aMessage.params[2])
          .writeMessage(aMessage.servername,
                        _("message.invited", aMessage.params[1],
                          aMessage.params[2]), {system: true});
      return true;
    },
    "342": function(aMessage) { // RPL_SUMMONING
      // <user> :Summoning user to IRC
      return writeMessage(this, aMessage,
                          _("message.summoned", aMessage.params[0]));
    },
    "346": function(aMessage) { // RPL_INVITELIST
      // <chanel> <invitemask>
      // TODO what do we do?
      return false;
    },
    "347": function(aMessage) { // RPL_ENDOFINVITELIST
      // <channel> :End of channel invite list
      // TODO what do we do?
      return false;
    },
    "348": function(aMessage) { // RPL_EXCEPTLIST
      // <channel> <exceptionmask>
      // TODO what do we do?
      return false;
    },
    "349": function(aMessage) { // RPL_ENDOFEXCEPTIONLIST
      // <channel> :End of channel exception list
      // TODO update UI?
      return false;
    },

    /*
     * Version
     */
    "351": function(aMessage) { // RPL_VERSION
      // <version>.<debuglevel> <server> :<comments>
      return serverMessage(this, aMessage);
    },

    /*
     * WHO
     */
    "352": function(aMessage) { // RPL_WHOREPLY
      // <channel> <user> <host> <server> <nick> ( "H" / "G" ) ["*"] [ ("@" / "+" ) ] :<hopcount> <real name>
      // TODO parse and display this?
      return false;
    },

    /*
     * NAMREPLY
     */
    "353": function(aMessage) { // RPL_NAMREPLY
      // <target> ( "=" / "*" / "@" ) <channel> :[ "@" / "+" ] <nick> *( " " [ "@" / "+" ] <nick> )
      let conversation = this.getConversation(aMessage.params[2]);
      // Keep if this is secret (@), private (*) or public (=).
      conversation.setModesFromRestriction(aMessage.params[1]);
      // Add the participants.
      let newParticipants = [];
      aMessage.params[3].trim().split(" ").forEach(function(aNick)
        newParticipants.push(conversation.getParticipant(aNick, false)));
      conversation.notifyObservers(new nsSimpleEnumerator(newParticipants),
                                   "chat-buddy-add");
      return true;
    },

    "361": function(aMessage) { // RPL_KILLDONE
      // Non-generic
      // TODO What is this?
      return false;
    },
    "362": function(aMessage) { // RPL_CLOSING
      // Non-generic
      // TODO What is this?
      return false;
    },
    "363": function(aMessage) { // RPL_CLOSEEND
      // Non-generic
      // TODO What is this?
      return false;
    },

    /*
     * Links.
     */
    "364": function(aMessage) { // RPL_LINKS
      // <mask> <server> :<hopcount> <server info>
      return serverMessage(this, aMessage);
    },
    "365": function(aMessage) { // RPL_ENDOFLINKS
      // <mask> :End of LINKS list
      return true;
    },

    /*
     * Names
     */
    "366": function(aMessage) { // RPL_ENDOFNAMES
      // <target> <channel> :End of NAMES list
      // All participants have already been added by the 353 handler.

      // This assumes that this is the last message received when joining a
      // channel, so a few "clean up" tasks are done here.
      let conversation = this.getConversation(aMessage.params[1]);
      // Update whether the topic is editable.
      conversation.checkTopicSettable();

      // If we haven't received the MODE yet, request it.
      if (!conversation._receivedInitialMode)
        this.sendMessage("MODE", aMessage.params[1]);

      return true;
    },
    /*
     * End of a bunch of lists
     */
    "367": function(aMessage) { // RPL_BANLIST
      // <channel> <banmask>
      // TODO
      return false;
    },
    "368": function(aMessage) { // RPL_ENDOFBANLIST
      // <channel> :End of channel ban list
      // TODO
      return false;
    },
    "369": function(aMessage) { // RPL_ENDOFWHOWAS
      // <nick> :End of WHOWAS
      // We've received everything about WHOWAS, tell the tooltip that is waiting
      // for this information.
      let nick = this.normalize(aMessage.params[1]);
      Services.obs.notifyObservers(this.getBuddyInfo(nick),
                                   "user-info-received", nick);
      return true;
    },

    /*
     * Server info
     */
    "371": function(aMessage) { // RPL_INFO
      // :<string>
      return serverMessage(this, aMessage);
    },
    "372": function(aMessage) { // RPL_MOTD
      // :- <text>
      this._motd.push(aMessage.params[1].slice(2));
      return true;
    },
    "373": function(aMessage) { // RPL_INFOSTART
      // Non-generic
      // This is unnecessary and servers just send RPL_INFO.
      return true;
    },
    "374": function(aMessage) { // RPL_ENDOFINFO
      // :End of INFO list
      return true;
    },
    "375": function(aMessage) { // RPL_MOTDSTART
      // :- <server> Message of the day -
      this._motd = [aMessage.params[1].slice(2, -2)];
      return true;
    },
    "376": function(aMessage) { // RPL_ENDOFMOTD
      // :End of MOTD command
      if (this._showServerTab)
        writeMessage(this, aMessage, this._motd.join("\n"), "incoming");
      delete this._motd;
      return true;
    },

    /*
     * OPER
     */
    "381": function(aMessage) { // RPL_YOUREOPER
      // :You are now an IRC operator
      // TODO update UI accordingly to show oper status
      return serverMessage(this, aMessage);
    },
    "382": function(aMessage) { // RPL_REHASHING
      // <config file> :Rehashing
      return serverMessage(this, aMessage);
    },
    "383": function(aMessage) { // RPL_YOURESERVICE
      // You are service <servicename>
      WARN("Received \"You are a service\" message.");
      return true;
    },

    /*
     * Info
     */
    "384": function(aMessage) { // RPL_MYPORTIS
      // Non-generic
      // TODO Parse and display?
      return false;
    },
    "391": function(aMessage) { // RPL_TIME
      // <server> :<string showing server's local time>
      // TODO parse date string & store or just show it?
      return false;
    },
    "392": function(aMessage) { // RPL_USERSSTART
      // :UserID   Terminal  Host
      // TODO
      return false;
    },
    "393": function(aMessage) { // RPL_USERS
      // :<username> <ttyline> <hostname>
      // TODO store into buddy list? List out?
      return false;
    },
    "394": function(aMessage) { // RPL_ENDOFUSERS
      // :End of users
      // TODO Notify observers of the buddy list?
      return false;
    },
    "395": function(aMessage) { // RPL_NOUSERS
      // :Nobody logged in
      // TODO clear buddy list?
      return false;
    },

      // Error messages, Implement Section 5.2 of RFC 2812
    "401": function(aMessage) { // ERR_NOSUCHNICK
      // <nickname> :No such nick/channel
      // Can arise in response to /mode, /invite, /kill, /msg, /whois.
      // TODO Handled in the conversation for /whois and /mgs so far.
      let msgId = "error.noSuch" +
        (this.isMUCName(aMessage.params[1]) ? "Channel" : "Nick");
      if (this.hasConversation(aMessage.params[1])) {
        // If the conversation exists and we just sent a message from it, then
        // notify that the user is offline.
        if (this.getConversation(aMessage.params[1])._pendingMessage)
          conversationErrorMessage(this, aMessage, msgId);
      }

      return serverErrorMessage(this, aMessage, _(msgId, aMessage.params[1]));
    },
    "402": function(aMessage) { // ERR_NOSUCHSERVER
      // <server name> :No such server
      // TODO Parse & display an error to the user.
      return false;
    },
    "403": function(aMessage) { // ERR_NOSUCHCHANNEL
      // <channel name> :No such channel
      delete this._chatRoomFieldsList[this.normalize(aMessage.params[1])];
      return conversationErrorMessage(this, aMessage, "error.noChannel");
    },
    "404": function(aMessage) { // ERR_CANNOTSENDTOCHAN
      // <channel name> :Cannot send to channel
      // Notify the user that they can't send to that channel.
      return conversationErrorMessage(this, aMessage,
                                      "error.cannotSendToChannel");
    },
    "405": function(aMessage) { // ERR_TOOMANYCHANNELS
      // <channel name> :You have joined too many channels
      delete this._chatRoomFieldsList[this.normalize(aMessage.params[1])];
      return serverErrorMessage(this, aMessage,
                                _("error.tooManyChannels", aMessage.params[1]));
    },
    "406": function(aMessage) { // ERR_WASNOSUCHNICK
      // <nickname> :There was no such nickname
      // Can arise in response to WHOWAS.
      return serverErrorMessage(this, aMessage,
                                _("error.wasNoSuchNick", aMessage.params[1]));
    },
    "407": function(aMessage) { // ERR_TOOMANYTARGETS
      // <target> :<error code> recipients. <abort message>
      delete this._chatRoomFieldsList[this.normalize(aMessage.params[1])];
      return conversationErrorMessage(this, aMessage, "error.nonUniqueTarget");
    },
    "408": function(aMessage) { // ERR_NOSUCHSERVICE
      // <service name> :No such service
      // TODO
      return false;
    },
    "409": function(aMessage) { // ERR_NOORIGIN
      // :No origin specified
      // TODO failed PING/PONG message, this should never occur?
      return false;
    },
    "411": function(aMessage) { // ERR_NORECIPIENT
      // :No recipient given (<command>)
      // If this happens a real error with the protocol occurred.
      ERROR("ERR_NORECIPIENT: No recipient given for PRIVMSG.");
      return true;
    },
    "412": function(aMessage) { // ERR_NOTEXTTOSEND
      // :No text to send
      // If this happens a real error with the protocol occurred: we should
      // always block the user from sending empty messages.
      ERROR("ERR_NOTEXTTOSEND: No text to send for PRIVMSG.");
      return true;
    },
    "413": function(aMessage) { // ERR_NOTOPLEVEL
      // <mask> :No toplevel domain specified
      // If this response is received, a real error occurred in the protocol.
      ERROR("ERR_NOTOPLEVEL: Toplevel domain not specified.");
      return true;
    },
    "414": function(aMessage) { // ERR_WILDTOPLEVEL
      // <mask> :Wildcard in toplevel domain
      // If this response is received, a real error occurred in the protocol.
      ERROR("ERR_WILDTOPLEVEL: Wildcard toplevel domain specified.");
      return true;
    },
    "415": function(aMessage) { // ERR_BADMASK
      // <mask> :Bad Server/host mask
      // If this response is received, a real error occurred in the protocol.
      ERROR("ERR_BADMASK: Bad server/host mask specified.");
      return true;
    },
    "421": function(aMessage) { // ERR_UNKNOWNCOMMAND
      // <command> :Unknown command
      // TODO This shouldn't occur
      return false;
    },
    "422": function(aMessage) { // ERR_NOMOTD
      // :MOTD File is missing
      // No message of the day to display.
      return true;
    },
    "423": function(aMessage) { // ERR_NOADMININFO
      // <server> :No administrative info available
      // TODO
      return false;
    },
    "424": function(aMessage) { // ERR_FILEERROR
      // :File error doing <file op> on <file>
      // TODO
      return false;
    },
    "431": function(aMessage) { // ERR_NONICKNAMEGIVEN
      // :No nickname given
      // TODO
      return false;
    },
    "432": function(aMessage) { // ERR_ERRONEUSNICKNAME
      // <nick> :Erroneous nickname
      let msg = _("error.erroneousNickname", aMessage.params[1]);
      serverErrorMessage(this, aMessage, msg);
      if (this._requestedNickname == this._accountNickname) {
        // The account has been set up with an illegal nickname.
        ERROR("Erroneous nickname " + aMessage.params[1] + ": " +
              aMessage.params[2]);
        this.gotDisconnected(Ci.prplIAccount.ERROR_INVALID_USERNAME, msg);
      }
      else {
        // Reset original nickname to the account nickname in case of
        // later reconnections.
        this._requestedNickname = this._accountNickname;
      }
      return true;
    },
    "433": function(aMessage) { // ERR_NICKNAMEINUSE
      // <nick> :Nickname is already in use
      return tryNewNick(this, aMessage);
    },
    "436": function(aMessage) { // ERR_NICKCOLLISION
      // <nick> :Nickname collision KILL from <user>@<host>
      return tryNewNick(this, aMessage);
    },
    "437": function(aMessage) { // ERR_UNAVAILRESOURCE
      // <nick/channel> :Nick/channel is temporarily unavailable
      // TODO
      delete this._chatRoomFieldsList[this.normalize(aMessage.params[1])];
      return false;
    },
    "441": function(aMessage) { // ERR_USERNOTINCHANNEL
      // <nick> <channel> :They aren't on that channel
      // TODO
      return false;
    },
    "442": function(aMessage) { // ERR_NOTONCHANNEL
      // <channel> :You're not on that channel
      // TODO
      return false;
    },
    "443": function(aMessage) { // ERR_USERONCHANNEL
      // <user> <channel> :is already on channel
      // TODO
      return false;
    },
    "444": function(aMessage) { // ERR_NOLOGIN
      // <user> :User not logged in
      // TODO
      return false;
    },
    "445": function(aMessage) { // ERR_SUMMONDISABLED
      // :SUMMON has been disabled
      // TODO keep track of this and disable UI associated?
      return false;
    },
    "446": function(aMessage) { // ERR_USERSDISABLED
      // :USERS has been disabled
      // TODO Disabled all buddy list etc.
      return false;
    },
    "451": function(aMessage) { // ERR_NOTREGISTERED
      // :You have not registered
      // TODO
      return false;
    },
    "461": function(aMessage) { // ERR_NEEDMOREPARAMS
      // <command> :Not enough parameters
      // TODO
      return false;
    },
    "462": function(aMessage) { // ERR_ALREADYREGISTERED
      // :Unauthorized command (already registered)
      // TODO
      return false;
    },
    "463": function(aMessage) { // ERR_NOPERMFORHOST
      // :Your host isn't among the privileged
      // TODO
      return false;
    },
    "464": function(aMessage) { // ERR_PASSWDMISMATCH
      // :Password incorrect
      // TODO prompt user for new password
      return false;
    },
    "465": function(aMessage) { // ERR_YOUREBANEDCREEP
      // :You are banned from this server
      serverErrorMessage(this, aMessage, _("error.banned"));
      this.gotDisconnected(Ci.prplIAccount.ERROR_OTHER_ERROR,
                           _("error.banned")); // Notify account manager.
      return true;
    },
    "466": function(aMessage) { // ERR_YOUWILLBEBANNED
      return serverErrorMessage(this, aMessage, _("error.bannedSoon"));
    },
    "467": function(aMessage) { // ERR_KEYSET
      // <channel> :Channel key already set
      // TODO
      return false;
    },
    "471": function(aMessage) { // ERR_CHANNELISFULL
      // <channel> :Cannot join channel (+l)
      // TODO
      delete this._chatRoomFieldsList[this.normalize(aMessage.params[1])];
      return false;
    },
    "472": function(aMessage) { // ERR_UNKNOWNMODE
      // <char> :is unknown mode char to me for <channel>
      // TODO
      return false;
    },
    "473": function(aMessage) { // ERR_INVITEONLYCHAN
      // <channel> :Cannot join channel (+i)
      // TODO
      delete this._chatRoomFieldsList[this.normalize(aMessage.params[1])];
      return false;
    },
    "474": function(aMessage) { // ERR_BANNEDFROMCHAN
      // <channel> :Cannot join channel (+b)
      // TODO
      delete this._chatRoomFieldsList[this.normalize(aMessage.params[1])];
      return false;
    },
    "475": function(aMessage) { // ERR_BADCHANNELKEY
      // <channel> :Cannot join channel (+k)
      // TODO need to inform the user.
      delete this._chatRoomFieldsList[this.normalize(aMessage.params[1])];
      return false;
    },
    "476": function(aMessage) { // ERR_BADCHANMASK
      // <channel> :Bad Channel Mask
      // TODO
      delete this._chatRoomFieldsList[this.normalize(aMessage.params[1])];
      return false;
    },
    "477": function(aMessage) { // ERR_NOCHANMODES
      // <channel> :Channel doesn't support modes
      // TODO
      return false;
    },
    "478": function(aMessage) { // ERR_BANLISTFULL
      // <channel> <char> :Channel list is full
      // TODO
      return false;
    },
    "481": function(aMessage) { // ERR_NOPRIVILEGES
      // :Permission Denied- You're not an IRC operator
      // TODO ask to auth?
      return false;
    },
    "482": function(aMessage) { // ERR_CHANOPRIVSNEEDED
      // <channel> :You're not channel operator
      return conversationErrorMessage(this, aMessage, "error.notChannelOp");
    },
    "483": function(aMessage) { // ERR_CANTKILLSERVER
      // :You can't kill a server!
      // TODO Display error?
      return false;
    },
    "484": function(aMessage) { // ERR_RESTRICTED
      // :Your connection is restricted!
      // Indicates user mode +r
      // TODO
      return false;
    },
    "485": function(aMessage) { // ERR_UNIQOPPRIVSNEEDED
      // :You're not the original channel operator
      // TODO ask to auth?
      return false;
    },
    "491": function(aMessage) { // ERR_NOOPERHOST
      // :No O-lines for your host
      // TODO
      return false;
    },
    "492": function(aMessage) { //ERR_NOSERVICEHOST
      // Non-generic
      // TODO
      return false;
    },
    "501": function(aMessage) { // ERR_UMODEUNKNOWNFLAGS
      // :Unknown MODE flag
      // TODO Display error?
      return false;
    },
    "502": function(aMessage) { // ERR_USERSDONTMATCH
      // :Cannot change mode for other users
      return serverErrorMessage(this, aMessage, _("error.mode.wrongUser"));
    }
  }
};
