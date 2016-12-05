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

# Chaos
chaos_obj		:=
chaos_obj		+= bin/chaos.o
chaos_obj		+= lib/chaos/cmdline.o
chaos_obj		+= lib/chaos/config.o

$(eval $(call smk_binary,chaos,$(chaos_obj)))
$(eval $(call smk_depend,chaos,h2))

$(chaos_bin): LDFLAGS += -ljansson -lh2
$(chaos_bin): LDFLAGS += $(XEN_LDFLAGS)
$(chaos_obj): CFLAGS += $(XEN_CFLAGS)

# LibH2
libh2_obj		:=
libh2_obj		+= lib/h2/xen/xc.o
libh2_obj		+= lib/h2/xen/dev.o
libh2_obj		+= lib/h2/xen/vif.o
libh2_obj		+= lib/h2/xen/xs.o
libh2_obj		+= lib/h2/xen/console.o
libh2_obj		+= lib/h2/h2.o
libh2_obj		+= lib/h2/xen.o
ifeq ($(CONFIG_H2_XEN_NOXS),y)
libh2_obj		+= lib/h2/xen/noxs.o
endif

$(eval $(call smk_library,h2,$(LIBH2_V_MAJOR),$(LIBH2_V_MINOR),$(LIBH2_V_BUGFIX),$(libh2_obj)))

$(libh2_so): LDFLAGS += -lxenctrl -lxenstore -lxenguest -lxentoollog -lxenforeignmemory
$(libh2_so): LDFLAGS += $(XEN_LDFLAGS)
$(libh2_obj): CFLAGS += $(XEN_CFLAGS)
