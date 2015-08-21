/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tries to find a configuration for this ISP on the local harddisk, in the
 * application install directory's "isp" subdirectory.
 * Params @see fetchConfigFromISP()
 */

Components.utils.import("resource://gre/modules/Services.jsm");

function fetchConfigFromDisk(domain, successCallback, errorCallback)
{
  return new TimeoutAbortable(runAsync(function()
  {
    try {
      // <TB installdir>/isp/example.com.xml
      var configLocation = Services.dirsvc.get("CurProcD", Ci.nsIFile);
      configLocation.append("isp");
      configLocation.append(sanitize.hostname(domain) + ".xml");

      var contents =
        readURLasUTF8(Services.io.newFileURI(configLocation));
       // Bug 336551 trips over <?xml ... >
      contents = contents.replace(/<\?xml[^>]*\?>/, "");
      successCallback(readFromXML(new XML(contents)));
    } catch (e) { errorCallback(e); }
  }));
}

/**
 * Tries to get a configuration from the ISP / mail provider directly.
 *
 * Disclaimers:
 * - To support domain hosters, we cannot use SSL. That means we
 *   rely on insecure DNS and http, which means the results may be
 *   forged when under attack. The same is true for guessConfig(), though.
 *
 * @param domain {String}   The domain part of the user's email address
 * @param emailAddress {String}   The user's email address
 * @param successCallback {Function(config {AccountConfig}})}   A callback that
 *         will be called when we could retrieve a configuration.
 *         The AccountConfig object will be passed in as first parameter.
 * @param errorCallback {Function(ex)}   A callback that
 *         will be called when we could not retrieve a configuration,
 *         for whatever reason. This is expected (e.g. when there's no config
 *         for this domain at this location),
 *         so do not unconditionally show this to the user.
 *         The first paramter will be an exception object or error string.
 */
function fetchConfigFromISP(domain, emailAddress, successCallback,
                            errorCallback)
{
  let url1 = "http://autoconfig." + sanitize.hostname(domain) +
             "/mail/config-v1.1.xml";
  // .well-known/ <http://tools.ietf.org/html/draft-nottingham-site-meta-04>
  let url2 = "http://" + sanitize.hostname(domain) +
             "/.well-known/autoconfig/mail/config-v1.1.xml";
  let sucAbortable = new SuccessiveAbortable();
  var time = Date.now();
  let fetch1 = new FetchHTTP(
    url1, { emailaddress: emailAddress }, false,
    function(result)
    {
      successCallback(readFromXML(result));
    },
    function(e1) // fetch1 failed
    {
      ddump("fetchisp 1 <" + url1 + "> took " + (Date.now() - time) +
          "ms and failed with " + e1);
      time = Date.now();
      if (e1 instanceof CancelledException)
      {
        errorCallback(e1);
        return;
      }

      let fetch2 = new FetchHTTP(
        url2, { emailaddress: emailAddress }, false,
        function(result)
        {
          successCallback(readFromXML(result));
        },
        function(e2)
        {
          ddump("fetchisp 2 <" + url2 + "> took " + (Date.now() - time) +
              "ms and failed with " + e2);
          // return the error for the primary call,
          // unless the fetch was cancelled
          errorCallback(e2 instanceof CancelledException ? e2 : e1);
        });
      sucAbortable.current = fetch2;
      fetch2.start();
    });
  sucAbortable.current = fetch1;
  fetch1.start();
  return sucAbortable;
}

/**
 * Tries to get a configuration for this ISP from a central database at
 * Mozilla servers.
 * Params @see fetchConfigFromISP()
 */

function fetchConfigFromDB(domain, successCallback, errorCallback)
{
  let pref = Cc["@mozilla.org/preferences-service;1"]
             .getService(Ci.nsIPrefBranch);
  let url = pref.getCharPref("mailnews.auto_config_url");
  domain = sanitize.hostname(domain);

  // If we don't specify a place to put the domain, put it at the end.
  if (url.indexOf("{{domain}}") == -1)
    url = url + domain;
  else
    url = url.replace("{{domain}}", domain);
  var accountManager = Cc["@mozilla.org/messenger/account-manager;1"]
                       .getService(Ci.nsIMsgAccountManager);
  url = url.replace("{{accounts}}", accountManager.accounts.Count());

  if (!url.length)
    return errorCallback("no fetch url set");
  let fetch = new FetchHTTP(url, null, false,
                            function(result)
                            {
                              //gEmailWizardLogger.info("result "+result);
                              successCallback(readFromXML(result));
                            },
                            errorCallback);
  fetch.start();
  return fetch;
}

/**
 * Does a lookup of DNS MX, to get the server who is responsible for
 * recieving mail for this domain. Then it takes the domain of that
 * server, and does another lookup (in ISPDB and possible at ISP autoconfig
 * server) and if such a config is found, returns that.
 *
 * Disclaimers:
 * - DNS is unprotected, meaning the results could be forged.
 *   The same is true for fetchConfigFromISP() and guessConfig(), though.
 * - DNS MX tells us the incoming server, not the mailbox (IMAP) server.
 *   They are different. This mechnism is only an approximation
 *   for hosted domains (yourname.com is served by mx.hoster.com and
 *   therefore imap.hoster.com - that "therefore" is exactly the
 *   conclusional jump we make here.) and alternative domains
 *   (e.g. yahoo.de -> yahoo.com).
 * - We make a look up for the base domain. E.g. if MX is
 *   mx1.incoming.servers.hoster.com, we look up hoster.com.
 *   Thanks to nsIEffectiveTLDService, we also get bbc.co.uk right.
 *
 * Params @see fetchConfigFromISP()
 */
function fetchConfigForMX(domain, successCallback, errorCallback)
{
  domain = sanitize.hostname(domain);

  var sucAbortable = new SuccessiveAbortable();
  var time = Date.now();
  sucAbortable.current = getMX(domain,
    function(mxHostname) // success
    {
      ddump("getmx took " + (Date.now() - time) + "ms");
      var tldServ = Cc["@mozilla.org/network/effective-tld-service;1"]
                      .getService(Ci.nsIEffectiveTLDService);
      var sld = tldServ.getBaseDomainFromHost(mxHostname);
      ddump("base domain " + sld + " for " + mxHostname);
      if (sld == domain)
      {
        errorCallback("MX lookup would be no different from domain");
        return;
      }
      sucAbortable.current = fetchConfigFromDB(sld, successCallback,
                                               errorCallback);
    },
    errorCallback);
  return sucAbortable;
}

/**
 * Queries the DNS MX for the domain
 *
 * The current implementation goes to a web service to do the
 * DNS resolve for us, because Mozilla unfortunately has no implementation
 * to do it. That's just a workaround. Once bug 545866 is fixed, we make
 * the DNS query directly on the client. The API of this function should not
 * change then.
 *
 * Returns (in successCallback) the hostname of the MX server.
 * If there are several entires with different preference values,
 * only the most preferred (i.e. those with the lowest value)
 * is returned. If there are several most preferred servers (i.e.
 * round robin), only one of them is returned.
 *
 * @param domain @see fetchConfigFromISP()
 * @param successCallback {function(hostname {String})
 *   Called when we found an MX for the domain.
 *   For |hostname|, see description above.
 * @param errorCallback @see fetchConfigFromISP()
 * @returns @see fetchConfigFromISP()
 */
function getMX(domain, successCallback, errorCallback)
{
  domain = sanitize.hostname(domain);

  let pref = Cc["@mozilla.org/preferences-service;1"]
               .getService(Ci.nsIPrefBranch);
  let url = pref.getCharPref("mailnews.mx_service_url");
  if (!url)
    errorCallback("no URL for MX service configured");
  url += domain;

  let fetch = new FetchHTTP(url, null, false,
    function(result)
    {
      // result is plain text, with one line per server.
      // So just take the first line
      ddump("MX query result: \n" + result + "(end)");
      assert(typeof(result) == "string");
      let first = result.split("\n")[0];
      first.toLowerCase().replace(/[^a-z0-9\-_\.]*/g, "");
      if (first.length == 0)
      {
        errorCallback("no MX found");
        return;
      }
      successCallback(first);
    },
    errorCallback);
  fetch.start();
  return fetch;
}
