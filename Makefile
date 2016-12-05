include smk/s.mk

# Default build flags
CFLAGS		+= -std=gnu11
CFLAGS		+= -Wall -g
ifeq ($(CONFIG_H2_XEN_NOXS),y)
CFLAGS		+= -DCONFIG_H2_XEN_NOXS
endif

ifneq ($(LINUX_HEADERS),)
CFLAGS		+= -I$(LINUX_HEADERS)
endif

XEN_CFLAGS	:=
XEN_CFLAGS	+= -I$(XEN_ROOT)/tools/include
XEN_CFLAGS	+= -I$(XEN_ROOT)/tools/libxc/include
XEN_CFLAGS	+= -I$(XEN_ROOT)/dist/install/$(XEN_PREFIX)/include

XEN_LDFLAGS	:=
XEN_LDFLAGS	+= -L$(XEN_ROOT)/dist/install/$(XEN_PREFIX)/lib
XEN_LDFLAGS	+= -Wl,-rpath-link,$(XEN_ROOT)/dist/install/$(XEN_PREFIX)/lib

include bin/chaos.mk
include lib/h2.mk
