# LibH2
libh2_obj		:=
libh2_obj		+= lib/h2/xen/xc.o
libh2_obj		+= lib/h2/xen/dev.o
libh2_obj		+= lib/h2/xen/sysctl.o
libh2_obj		+= lib/h2/xen/vif.o
libh2_obj		+= lib/h2/xen/xs.o
libh2_obj		+= lib/h2/xen/console.o
libh2_obj		+= lib/h2/h2.o
libh2_obj		+= lib/h2/xen.o
libh2_obj		+= lib/h2/stream.o
libh2_obj		+= lib/h2/os_stream_file.o
libh2_obj		+= lib/h2/os_stream_net.o
ifeq ($(CONFIG_H2_XEN_NOXS),y)
libh2_obj		+= lib/h2/xen/noxs.o
endif

$(eval $(call smk_library,h2,$(LIBH2_V_MAJOR),$(LIBH2_V_MINOR),$(LIBH2_V_BUGFIX),$(libh2_obj)))

$(libh2_so): LDFLAGS += -lxenctrl -lxenstore -lxenguest -lxentoollog -lxenforeignmemory
$(libh2_so): LDFLAGS += $(XEN_LDFLAGS)
$(libh2_obj): CFLAGS += $(XEN_CFLAGS)
