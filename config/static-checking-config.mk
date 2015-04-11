# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The entire tree should be subject to static analysis using the XPCOM
# script. Additional scripts may be added by specific subdirectories.

DEHYDRA_SCRIPT = $(MOZILLA_SRCDIR)/config/static-checking.js

DEHYDRA_MODULES = \
  $(MOZILLA_SRCDIR)/xpcom/analysis/final.js \
  $(MOZILLA_SRCDIR)/xpcom/analysis/override.js \
  $(MOZILLA_SRCDIR)/xpcom/analysis/must-override.js \
  $(NULL)

TREEHYDRA_MODULES = \
  $(MOZILLA_SRCDIR)/xpcom/analysis/outparams.js \
  $(MOZILLA_SRCDIR)/xpcom/analysis/stack.js \
  $(MOZILLA_SRCDIR)/xpcom/analysis/flow.js \
  $(MOZILLA_SRCDIR)/js/src/jsstack.js \
  $(MOZILLA_SRCDIR)/layout/generic/frame-verify.js \
  $(NULL)

DEHYDRA_ARGS = \
  --topsrcdir=$(topsrcdir) \
  --objdir=$(MOZDEPTH) \
  --dehydra-modules=$(subst $(NULL) ,$(COMMA),$(strip $(DEHYDRA_MODULES))) \
  --treehydra-modules=$(subst $(NULL) ,$(COMMA),$(strip $(TREEHYDRA_MODULES))) \
  $(NULL)

DEHYDRA_FLAGS = -fplugin=$(DEHYDRA_PATH) -fplugin-arg='$(DEHYDRA_SCRIPT) $(DEHYDRA_ARGS)'

ifdef DEHYDRA_PATH
OS_CXXFLAGS += $(DEHYDRA_FLAGS)
endif
