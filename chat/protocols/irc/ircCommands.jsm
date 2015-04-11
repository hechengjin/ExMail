/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is to be exported directly onto the IRC prplIProtocol object, directly
// implementing the commands field before we register them.
const EXPORTED_SYMBOLS = ["commands"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource:///modules/ircUtils.jsm");
Cu.import("resource:///modules/imServices.jsm");

// Shortcut to get the JavaScript conversation object.
function getConv(aConv) aConv.wrappedJSObject;

// Shortcut to get the JavaScript account object.
function getAccount(aConv) getConv(aConv)._account;

// Trim leading and trailing spaces and split a string by any type of space.
function splitInput(aString) aString.trim().split(/\s+/);

// Kick a user from a channel
// aMsg is <user> [comment]
function kickCommand(aMsg, aConv) {
  if (!aMsg.length)
    return false;

  let params = [aConv.name];
  let offset = aMsg.indexOf(" ");
  if (offset != -1) {
    params.push(aMsg.slice(0, offset));
    params.push(aMsg.slice(offset + 1));
  }
  else
    params.push(aMsg);

  getAccount(aConv).sendMessage("KICK", params);
  return true;
}

// Send a message directly to a user.
// aMsg is <user> <message>
function messageCommand(aMsg, aConv) {
  let sep = aMsg.indexOf(" ");
  // If no space in the message or the first space is at the end of the message.
  if (sep == -1 || (sep + 1) == aMsg.length) {
    let msg = aMsg.trim();
    if (!msg.length)
      return false;
    getAccount(aConv).createConversation(msg);
    return true;
  }

  return privateMessage(aConv, aMsg.slice(sep + 1), aMsg.slice(0, sep));
}

// aAdd is true to add a mode, false to remove a mode.
function setMode(aNickname, aConv, aMode, aAdd) {
  if (!aNickname.length)
    return false;

  // Change the mode for each nick, as separator by spaces.
  return splitInput(aNickname).every(function(aNick)
    simpleCommand(aConv, "MODE",
                  [aConv.name, (aAdd ? "+" : "-") + aMode, aNick]));
}

function actionCommand(aMsg, aConv) {
  // Don't try to send an empty action.
  if (!aMsg || !aMsg.trim().length)
    return false;
  if (!ctcpCommand(aConv, aConv.name, "ACTION", aMsg))
    return false;

  // Show the action on our conversation.
  getConv(aConv).writeMessage(getAccount(aConv)._nickname, "/me " + aMsg,
                              {outgoing: true});
  return true;
}

// Helper functions
function privateMessage(aConv, aMsg, aNickname) {
  if (!aMsg.length)
    return false;

  // This will open the conversation, send and display the text
  getAccount(aConv).getConversation(aNickname).sendMsg(aMsg);
  return true;
}

// This will send a command to the server, if no parameters are given, it is
// assumed that the command takes no parameters. aParams can be either a single
// string or an array of parameters.
function simpleCommand(aConv, aCommand, aParams) {
  if (!aParams || !aParams.length)
    getAccount(aConv).sendMessage(aCommand);
  else
    getAccount(aConv).sendMessage(aCommand, aParams);
  return true;
}

function ctcpCommand(aConv, aTarget, aCommand, aMsg) {
  if (!aTarget.length)
    return false;

  getAccount(aConv).sendCTCPMessage(aCommand, aMsg, aTarget, false);
  return true;
}

var commands = [
  {
    name: "action",
    get helpString() _("command.action", "action"),
    run: actionCommand
  },
  {
    name: "ctcp",
    get helpString() _("command.ctcp", "ctcp"),
    run: function(aMsg, aConv) {
      let separator = aMsg.indexOf(" ");
      if (separator == -1 && (separator + 1) != aMsg.length)
        return false;

      return ctcpCommand(aConv, aMsg.slice(0, separator),
                         aMsg.slice(separator + 1));
    }
  },
  {
    name: "chanserv",
    get helpString() _("command.chanserv", "chanserv"),
    run: function(aMsg, aConv) privateMessage(aConv, aMsg, "ChanServ")
  },
  {
    name: "deop",
    get helpString() _("command.deop", "deop"),
    run: function(aMsg, aConv) setMode(aMsg, aConv, "o", false)
  },
  {
    name: "devoice",
    get helpString() _("command.devoice", "devoice"),
    run: function(aMsg, aConv) setMode(aMsg, aConv, "v", false)
  },
  {
    name: "invite",
    get helpString() _("command.invite", "invite"),
    run: function(aMsg, aConv) {
      let params = splitInput(aMsg);
      // If no parameters are given.
      if (!params[0].length)
        return false;
      // If only a nick is given, append the current channel name.
      if (params.length == 1)
        params.push(aConv.name);

      return simpleCommand(aConv, "INVITE", params);
    }
  },
  {
    name: "join",
    get helpString() _("command.join", "join"),
    run: function(aMsg, aConv) {
      let params = aMsg.trim().split(/,\s*/);
      if (!params[0])
        return false;
      let account = getAccount(aConv);
      params.forEach(function(joinParam) {
        if (joinParam)
          account.joinChat(account.getChatRoomDefaultFieldValues(joinParam));
      });
      return true;
    }
  },
  {
    name: "kick",
    get helpString() _("command.kick", "kick"),
    run: kickCommand
  },
  {
    name: "list",
    get helpString() _("command.list", "list"),
    run: function(aMsg, aConv) simpleCommand(aConv, "LIST")
  },
  {
    name: "me",
    get helpString() _("command.action", "me"),
    run: actionCommand
  },
  {
    name: "memoserv",
    get helpString() _("command.memoserv", "memoserv"),
    run: function(aMsg, aConv) privateMessage(aConv, aMsg, "MemoServ")
  },
  {
    name: "mode",
    get helpString() _("command.mode", "mode"),
    run: function(aMsg, aConv) {
      function isMode(aString) "+-".indexOf(aString[0]) != -1;
      let params = splitInput(aMsg);

      // Check if we have any params, we can't just check params.length, since
      // that will always be at least 1 (but params[0] would be empty).
      let hasParams = !/^\s*$/.test(aMsg);
      let account = getAccount(aConv);
      // These must be false if we don't have any paramters!
      let isChannelName = hasParams && account.isMUCName(params[0]);
      let isOwnNick =
        account.normalize(params[0]) == account.normalize(account._nickname);

      // If no parameters are given, the user is requesting their own mode.
      if (!hasParams)
        params = [aConv.nick];
      else if (params.length == 1) {
        // Only a mode is given, therefore the user is trying to set their own
        // mode. We need to provide the user's nick.
        if (isMode(params[0]))
          params.unshift(aConv.nick);
        // Alternately if the user gives a channel name, they're requesting a
        // channel's mode. If they give their own nick, they're requesting their
        // own mode. Otherwise, this is nonsensical.
        else if (!isChannelName && !isOwnNick)
          return false;
      }
      else if (params.length == 2) {
        // If a new mode and a nick are given, then we need to provide the
        // current conversation's name.
        if (isMode(params[0]) && !isMode(params[1]))
          params = [aConv.name, params[0], params[1]];
        // Otherwise, the input must be a channel name or the user's own nick
        // and a mode.
        else if ((!isChannelName && !isOwnNick) || !isMode(params[1]))
          return false;
      }
      // Otherwise a channel name, new mode and a nick were given or a channel
      // name and a mode were given. If this is not true, return false.
      else if (!(isChannelName && isMode(params[1])))
        return false;

      return simpleCommand(aConv, "MODE", params);
    }
  },
  {
    name: "msg",
    get helpString() _("command.msg", "msg"),
    run: messageCommand
  },
  {
    name: "nick",
    get helpString() _("command.nick", "nick"),
    run: function(aMsg, aConv) {
      let newNick = aMsg.trim();
      if (newNick.indexOf(/\s+/) != -1)
        return false;
      // The user wants to change their nick, so overwrite the account
      // nickname for this session.
      getAccount(aConv)._requestedNickname = newNick;
      return simpleCommand(aConv, "NICK", newNick);
    }
  },
  {
    name: "nickserv",
    get helpString() _("command.nickserv", "nickserv"),
    run: function(aMsg, aConv) privateMessage(aConv, aMsg, "NickServ")
  },
  {
    name: "notice",
    get helpString() _("command.notice", "notice"),
    run: function(aMsg, aConv) simpleCommand(aConv, "NOTICE", aMsg)
  },
  {
    name: "op",
    get helpString() _("command.op", "op"),
    run: function(aMsg, aConv) setMode(aMsg, aConv, "o", true)
  },
  {
    name: "operwall",
    get helpString() _("command.wallops", "operwall"),
    run: function(aMsg, aConv) simpleCommand(aConv, "WALLOPS", aMsg)
  },
  {
    name: "operserv",
    get helpString() _("command.operserv", "operserv"),
    run: function(aMsg, aConv) privateMessage(aConv, aMsg, "OperServ")
  },
  {
    name: "part",
    get helpString() _("command.part", "part"),
    run: function (aMsg, aConv) {
      getConv(aConv).part(aMsg);
      return true;
    }
  },
  {
    name: "ping",
    get helpString() _("command.ping", "ping"),
    run: function(aMsg, aConv) ctcpCommand(aConv, aMsg, "PING")
  },
  {
    name: "query",
    get helpString() _("command.msg", "query"),
    run: messageCommand
  },
  {
    name: "quit",
    get helpString() _("command.quit", "quit"),
    run: function(aMsg, aConv) {
      getAccount(aConv).quit(aMsg);
      return true;
    }
  },
  {
    name: "quote",
    get helpString() _("command.quote", "quote"),
    run: function(aMsg, aConv) {
      if (!aMsg.length)
        return false;

      getAccount(aConv).sendRawMessage(aMsg);
      return true;
    }
  },
  {
    name: "remove",
    get helpString() _("command.kick", "remove"),
    run: kickCommand
  },
  {
    name: "time",
    get helpString() _("command.time", "time"),
    run: function(aMsg, aConv) simpleCommand(aConv, "TIME")
  },
  {
    name: "topic",
    get helpString() _("command.topic", "topic"),
    run: function(aMsg, aConv) {
      aConv.topic = aMsg;
      return true;
    }
  },
  {
    name: "umode",
    get helpString() _("command.umode", "umode"),
    run: function(aMsg, aConv) simpleCommand(aConv, "MODE", aMsg)
  },
  {
    name: "version",
    get helpString() _("command.version", "version"),
    run: function(aMsg, aConv) ctcpCommand(aConv, aMsg, "VERSION")
  },
  {
    name: "voice",
    get helpString() _("command.voice", "voice"),
    run: function(aMsg, aConv) setMode(aMsg, aConv, "v", true)
  },
  {
    name: "wallops",
    get helpString() _("command.wallops", "wallops"),
    run: function(aMsg, aConv) simpleCommand(aConv, "WALLOPS", aMsg)
  },
  {
    name: "whois",
    get helpString() _("command.whois", "whois"),
    run: function(aMsg, aConv) {
      // Note that this will automatically run whowas is the nick is offline.
      aMsg = aMsg.trim();
      if (!aMsg || aMsg.indexOf(" ") != -1)
        return false;
      getConv(aConv).requestBuddyInfo(aMsg);
      return true;
    }
  }
];
