# Chaos
chaos_obj		:=
chaos_obj		+= bin/chaos.o
chaos_obj		+= lib/chaos/cmdline.o

$(eval $(call smk_binary,chaos,$(chaos_obj)))
$(eval $(call smk_depend,chaos,h2))

$(chaos_bin): LDFLAGS += -lh2
$(chaos_bin): LDFLAGS += $(XEN_LDFLAGS)
$(chaos_obj): CFLAGS += $(XEN_CFLAGS)

# Daemon
daemon_obj		:=
daemon_obj		+= bin/daemon.o
daemon_obj		+= lib/daemon/cmdline.o

$(eval $(call smk_binary,daemon,$(daemon_obj)))
$(eval $(call smk_depend,daemon,h2))

$(daemon_bin): LDFLAGS += -lh2
$(daemon_bin): LDFLAGS += $(XEN_LDFLAGS)
$(daemon_obj): CFLAGS += $(XEN_CFLAGS)
