# Chaos
chaos_obj		:=
chaos_obj		+= bin/chaos.o
chaos_obj		+= lib/chaos/cmdline.o

$(eval $(call smk_binary,chaos,$(chaos_obj)))
$(eval $(call smk_depend,chaos,h2))

$(chaos_bin): LDFLAGS += -lh2
$(chaos_bin): LDFLAGS += $(XEN_LDFLAGS)
$(chaos_obj): CFLAGS += $(XEN_CFLAGS)

# Daemon for restore functionality
restore_daemon_obj	:=
restore_daemon_obj	+= bin/restore_daemon.o
restore_daemon_obj	+= lib/restore_daemon/cmdline.o

$(eval $(call smk_binary,restore_daemon,$(restore_daemon_obj)))
$(eval $(call smk_depend,restore_daemon,h2))

$(restore_daemon_bin): LDFLAGS += -lh2
$(restore_daemon_bin): LDFLAGS += $(XEN_LDFLAGS)
$(restore_daemon_obj): CFLAGS += $(XEN_CFLAGS)
