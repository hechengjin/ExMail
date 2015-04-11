#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Runs the Bloat test harness
"""

import optparse
import sys
import os
import shutil

from automation import Automation
automation = Automation()

from automationutils import checkForCrashes

class BloatRunTestOptions(optparse.OptionParser):
    """Parses Bloat runtest.py commandline options."""
    def __init__(self, **kwargs):
        optparse.OptionParser.__init__(self, **kwargs)
        defaults = {}

        self.add_option("--distdir",
                        action = "store", type = "string", dest = "distdir",
                        help = "object directory of build to run")
        defaults["distdir"] = "distdir-tb"

        self.add_option("--bin",
                        action = "store", type = "string", dest = "bin",
                        help = "application binary name")
        defaults["bin"] = "thunderbird"

        self.add_option("--brand",
                        action = "store", type = "string", dest = "brand",
                        help = "The current branding, including Debug if necessary")
        defaults["brand"] = "Daily"

        self.add_option("--symbols-path",
                        action = "store", type = "string", dest = "symbols",
                        help = "The path to the symbol files from build_symbols")
        defaults["symbols"] = ""

        self.add_option("--extra-startup-arg",
                        action = "store", type = "string", dest = "extraArg",
                        help = "Extra startup argument if required, at the moment this will only support one extra argument with no parameters")
        defaults["extraArg"] = ""

        self.set_defaults(**defaults);

        usage = """\
Usage instructions for runtest.py.
All arguments must be specified.
"""
        self.set_usage(usage)


parser = BloatRunTestOptions()
options, args = parser.parse_args()

if options.distdir == "" or options.bin == "" or options.brand == "":
  parser.print_help()
  sys.exit(1)

DISTDIR = os.path.abspath(os.path.realpath(options.distdir))
print DISTDIR

CWD = os.getcwd()
SCRIPTDIR = os.path.abspath(os.path.realpath(os.path.dirname(sys.argv[0])))

if automation.IS_MAC:
  APPBUNDLE = options.brand + '.app'
  BINDIR = os.path.join(DISTDIR, APPBUNDLE, 'Contents', 'MacOS')
else:
  BINDIR = os.path.join(DISTDIR, 'bin')

BIN = os.path.join(BINDIR, options.bin)
EXTENSIONDIR = os.path.join(DISTDIR, '..', '_tests', 'mailbloat', 'mailbloat')
PROFILE = os.path.join(DISTDIR, '..', '_tests', 'mailbloat', 'leakprofile')
print BIN

# Wipe the profile
if os.path.exists(PROFILE):
  shutil.rmtree(PROFILE)
os.mkdir(PROFILE)

defaultEnv = automation.environment()
defaultEnv['NO_EM_RESTART'] = '1'
if (not "XPCOM_DEBUG_BREAK" in defaultEnv):
  defaultEnv['XPCOM_DEBUG_BREAK'] = 'stack'

defaultArgs = ['-no-remote']
if automation.IS_MAC:
  defaultArgs.append('-foreground')

COMMANDS = [
  {
    'name': 'register',
    'args': ['-register'],
  },
  {
    'name': 'createProfile',
    'args': ['-CreateProfile', 'bloat ' + PROFILE],
  },
  {
    'name': 'setupProfile'
  },
  {
   'name': 'bloatTests',
   'args': ['-profile', PROFILE],
   'env': {'XPCOM_MEM_BLOAT_LOG': 'bloat.log'},
  },
  {
   'name': 'leakTests',
   'args': ['-profile',         PROFILE,
            '--trace-malloc',   'malloc.log',
            '--shutdown-leaks', 'sdleak.log',
           ],
   'env': {'XPCOM_MEM_BLOAT_LOG': 'trace-bloat.log'},
  },
]


for cmd in COMMANDS:
  # Some scripts rely on the cwd
  cwd = CWD
  if 'cwd' in cmd:
    cwd = cmd['cwd']
  os.chdir(cwd)

  if cmd['name'] == 'setupProfile':
      automation.installExtension(EXTENSIONDIR, PROFILE, "mailbloat@mozilla.org")
      print "Hello"
      continue

  # Set up the environment
  mailnewsEnv = defaultEnv
  if 'env' in cmd:
    mailnewsEnv.update(cmd['env'])

  # Build the command
  binary = BIN
  # Copy default args, using the : option.
  args = defaultArgs[:]
  args.extend(cmd['args'])

  if options.extraArg != "":
    args.append(options.extraArg)

  # Different binary implies no default args
  if 'bin' in cmd:
    binary = cmd['bin']
    args = cmd['args']

  print >> sys.stderr, 'INFO | runtest.py | Running ' + cmd['name'] + ' in ' + CWD + ' : '
  print >> sys.stderr, 'INFO | runtest.py | ', binary, args
  envkeys = mailnewsEnv.keys()
  envkeys.sort()
  for envkey in envkeys:
    print >> sys.stderr, "%s=%s"%(envkey, mailnewsEnv[envkey])

  proc = automation.Process([binary] + args, env = mailnewsEnv)

  status = proc.wait()
  if status != 0:
    print >> sys.stderr, "TEST-UNEXPECTED-FAIL | runtest.py | Exited with code %d during test run"%(status)

  if checkForCrashes(os.path.join(PROFILE, "minidumps"), options.symbols, cmd['name']):
    print >> sys.stderr, 'TinderboxPrint: ' + cmd['name'] + '<br/><em class="testfail">CRASH</em>'
    status = 1

  if status != 0:
    sys.exit(status)

  print >> sys.stderr, 'INFO | runtest.py | ' + cmd['name'] + ' executed successfully.'

print >> sys.stderr, 'INFO | runtest.py | All tests executed successfully.'
