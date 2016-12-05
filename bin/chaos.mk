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
