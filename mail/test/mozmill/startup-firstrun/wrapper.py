# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# For these tests, we need to disable the account provisioner, or
# else it will spawn immediately and block before we have a chance to run
# any Mozmill tests.

import os
import shutil
import sys

# We don't want any accounts for these tests.
NO_ACCOUNTS = True

def on_profile_created(profiledir):
    """
    On profile creation, this copies prefs.js from the current folder to
    profile_dir/preferences. This is a somewhat undocumented feature -- anything
    in profile_dir/preferences gets treated as a default pref, which is what we
    want here.
    """
    prefdir = os.path.join(profiledir, "preferences")
    # This needs to be a directory, so if it's a file, raise an exception
    if os.path.isfile(prefdir):
        raise Exception("%s needs to be a directory, but is a file" % prefdir)
    if not os.path.exists(prefdir):
        os.mkdir(prefdir)
    # The pref file is in the same directory this script is in
    # Fallback to Linux prefs for anything not in the dictionary.
    preffile = os.path.join(os.path.dirname(__file__), "prefs.js")
    shutil.copy(preffile, prefdir)
