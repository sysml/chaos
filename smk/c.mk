CFLAGS		+= -Iinc
CFLAGS		+= -MD -MP
LDFLAGS		+= -Llib

cc	 = $(call cmd, "CC", $2, $(CC), $(CFLAGS) -c $(1) -o $(2))
ld	 = $(call cmd, "LD", $2, $(CC), $(CFLAGS) $(1) $(LDFLAGS) -o $(2))
ar	 = $(call cmd, "AR", $2, ar rcs, $(2) $(1))

define __smk_add_compile_rule
%.o: %.c $$(config)
	$$(call cc, $$<, $$@)
endef
