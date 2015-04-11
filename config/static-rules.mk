# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


ifdef _NO_AUTO_VARS
_TARGET = $(srcdir)/$(@F)
else
_TARGET = $@
endif

$(warning FINAL_LINK_COMP_NAMES = $(FINAL_LINK_COMP_NAMES))
$(warning FINAL_LINK_COMPS = $(FINAL_LINK_COMPS))

nsStaticComponents.cpp: $(MOZILLA_SRCDIR)/config/nsStaticComponents.cpp.in $(GLOBAL_DEPS) $(FINAL_LINK_COMP_NAMES)
	rm -f $@
	cat $< | \
	sed -e "s|%MODULE_LIST%|$(foreach m, $(STATIC_COMPONENT_LIST),MODULE($(m)))|" \
	> $(_TARGET)

GARBAGE += nsStaticComponents.cpp

ifeq ($(OS_ARCH),IRIX)
LDFLAGS	+= -Wl,-LD_LAYOUT:lgot_buffer=80
endif

ifeq (cocoa,$(MOZ_WIDGET_TOOLKIT))
LIBS	+= -framework QuickTime -framework IOKit -lcrypto
endif
