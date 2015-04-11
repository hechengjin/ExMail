/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  let manifest = { // normal provider
    name: "provider 1",
    origin: "https://example.com",
    sidebarURL: "https://example.com/browser/browser/base/content/test/social_sidebar.html",
    workerURL: "https://example.com/browser/browser/base/content/test/social_worker.js",
    iconURL: "https://example.com/browser/browser/base/content/test/moz.png"
  };
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function () {
      let chats = document.getElementById("pinnedchats");
      ok(chats.children.length == 0, "no chatty children left behind");
      finishcb();
    });
  });
}

var tests = {
  testOpenCloseChat: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-sidebar-message":
          port.postMessage({topic: "test-chatbox-open"});
          break;
        case "got-chatbox-visibility":
          if (e.data.result == "hidden") {
            ok(true, "chatbox got minimized");
            chats.selectedChat.toggle();
          } else if (e.data.result == "shown") {
            ok(true, "chatbox got shown");
            // close it now
            let iframe = chats.selectedChat.iframe;
            iframe.addEventListener("unload", function chatUnload() {
              iframe.removeEventListener("unload", chatUnload, true);
              ok(true, "got chatbox unload on close");
              port.close();
              next();
            }, true);
            chats.selectedChat.close();
          }
          break;
        case "got-chatbox-message":
          ok(true, "got chatbox message");
          ok(e.data.result == "ok", "got chatbox windowRef result: "+e.data.result);
          chats.selectedChat.toggle();
          break;
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },
  testManyChats: function(next) {
    // open enough chats to overflow the window, then check
    // if the menupopup is visible
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.postMessage({topic: "test-init"});
    let width = document.documentElement.boxObject.width;
    let numToOpen = (width / 200) + 1;
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-chatbox-message":
          numToOpen--;
          if (numToOpen >= 0) {
            // we're waiting for all to open
            ok(true, "got a chat window opened");
            break;
          }
          // close our chats now
          let chats = document.getElementById("pinnedchats");
          ok(!chats.menupopup.parentNode.collapsed, "menu selection is visible");
          while (chats.selectedChat) {
            chats.selectedChat.close();
          }
          ok(!chats.selectedChat, "chats are all closed");
          port.close();
          next();
          break;
      }
    }
    let num = numToOpen;
    while (num-- > 0) {
      port.postMessage({topic: "test-chatbox-open", data: { id: num }});
    }
  },
  testWorkerChatWindow: function(next) {
    const chatUrl = "https://example.com/browser/browser/base/content/test/social_chat.html";
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.postMessage({topic: "test-init"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-chatbox-message":
          ok(true, "got a chat window opened");
          let chats = document.getElementById("pinnedchats");
          ok(chats.selectedChat.minimized, "chatbox from worker opened as minimized");
          while (chats.selectedChat) {
            chats.selectedChat.close();
          }
          ok(!chats.selectedChat, "chats are all closed");
          port.close();
          ensureSocialUrlNotRemembered(chatUrl);
          next();
          break;
      }
    }
    port.postMessage({topic: "test-worker-chat", data: chatUrl});
  },
  testCloseSelf: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-chatbox-open"});
          break;
        case "got-chatbox-visibility":
          is(e.data.result, "shown", "chatbox shown");
          port.close(); // don't want any more visibility messages.
          let chat = chats.selectedChat;
          ok(chat.parentNode, "chat has a parent node before it is closed");
          // ask it to close itself.
          let doc = chat.iframe.contentDocument;
          let evt = doc.createEvent("CustomEvent");
          evt.initCustomEvent("socialTest-CloseSelf", true, true, {});
          doc.documentElement.dispatchEvent(evt);
          ok(!chat.parentNode, "chat is now closed");
          next();
          break;
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },
  testSameChatCallbacks: function(next) {
    let chats = document.getElementById("pinnedchats");
    let port = Social.provider.getWorkerPort();
    let seen_opened = false;
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "test-init-done":
          port.postMessage({topic: "test-chatbox-open"});
          break;
        case "chatbox-opened":
          is(e.data.result, "ok", "the sidebar says it got a chatbox");
          if (seen_opened) {
            // This is the second time we've seen this message - there should
            // be exactly 1 chat open.
            let chats = document.getElementById("pinnedchats");
            chats.selectedChat.close();
            is(chats.selectedChat, null, "should only have been one chat open");
            next();
          } else {
            // first time we got the opened message, so re-request the same
            // chat to be opened - we should get the message again.
            seen_opened = true;
            port.postMessage({topic: "test-chatbox-open"});
          }
      }
    }
    port.postMessage({topic: "test-init", data: { id: 1 }});
  },
  testCloseOnLogout: function(next) {
    const chatUrl = "https://example.com/browser/browser/base/content/test/social_chat.html";
    let port = Social.provider.getWorkerPort();
    ok(port, "provider has a port");
    port.postMessage({topic: "test-init"});
    port.onmessage = function (e) {
      let topic = e.data.topic;
      switch (topic) {
        case "got-chatbox-message":
          ok(true, "got a chat window opened");
          port.postMessage({topic: "test-logout"});
          waitForCondition(function() document.getElementById("pinnedchats").firstChild == null,
                           next,
                           "chat windows didn't close");
          break;
      }
    }
    port.postMessage({topic: "test-worker-chat", data: chatUrl});
  }
}
