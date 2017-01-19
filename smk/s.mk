# SMK version
smk_version		:= 0x000100

# Basic configurations
verbose			?= n
smk_lang		?= c
smk_dir			?= smk
smk_conf_file	?= .config
smk_conf_script	?= ./configure

# Include default targets
include $(smk_dir)/targets.mk

# Include default build rules and commands
include $(smk_dir)/rules.mk

# Include configuration mechanism
include $(smk_dir)/config.mk

# Include language specific build rules
ifeq (c,$(smk_lang))
include $(smk_dir)/c.mk
else
ifeq (c++,$(smk_lang))
include $(smk_dir)/cxx.mk
else
$(error "Non-supported language $(smk_lang)")
endif
endif

# Include cscope and ctags support
include $(smk_dir)/tags.mk

# Provide smk functions
define smk_binary
$(1)_bin	:= bin/$(1)

$(1): bin/$(1)

all: $(1)
.PHONY: $(1)

$(1)_install: bin/$(1)
	$$(call cmd, "INSTALL", $(1), cp -f, bin/$(1) $$(PREFIX)/bin/$(1))

install: $(1)_install
.PHONY: $(1)_install

$(1)_uninstall:
	$$(call cmd, "UNINSTALL", rm -f, $$(PREFIX)/bin/$(1))

uninstall: $(1)_uninstall
.PHONY: $(1)_uninstall

$(1)_clean:
	$$(call clean, "$(1) *.o", $(2))
	$$(call clean, "$(1) *.d", $$(patsubst %.o,%.d,$(2)))

clean: $(1)_clean
.PHONY: $(1)_clean

$(1)_distclean:
	$$(call clean, $(1), bin/$(1))

distclean: $(1)_distclean
.PHONY: $(1)_distclean

bin/$(1): $(2)
	$$(call ld, $(2), $$@)

$(eval $(call __smk_add_compile_rule, $(2)))

-include $$(patsubst %.o,%.d,$(2))
endef

define smk_library
lib$(1)_a	:= lib/lib$(1).a
lib$(1)_so	:= lib/lib$(1).so

l_a			:= lib$(1).a
l_so		:= lib$(1).so
l_so_m		:= $$(l_so).$(2)
l_so_mm		:= $$(l_so).$(2).$(3)
l_so_mmb	:= $$(l_so).$(2).$(3).$(4)

lib$(1): lib/$$(l_so) lib/$$(l_a)

all: lib$(1)
.PHONY: lib$(1)

lib$(1)_install: lib$(1)
	$(call cmd, "INSTALL" , $(1)       , cp -r , inc/$(1)    $$(PREFIX)/include/)
	$(call cmd, "INSTALL" , $$(l_so_mmb), cp -f , lib/$$(l_so) $$(PREFIX)/lib/$$(l_so_mmb))
	$(call cmd, "INSTALL" , $$(l_so_m)  , ln -sf, $$(l_so_mmb) $$(PREFIX)/lib/$$(l_so_m))
	$(call cmd, "INSTALL" , $$(l_so)    , ln -sf, $$(l_so_mmb) $$(PREFIX)/lib/$$(l_so))
	$(call cmd, "INSTALL" , $$(l_a)     , cp -f , lib/$$(l_a)  $$(PREFIX)/lib/$$(l_a))
	$(call cmd, "LDCONFIG", ""         , ldconfig)

install: lib$(1)_install
.PHONY: lib$(1)_install

lib$(1)_uninstall:
	$(call cmd, "UNINSTALL", $(1)       , rm -rf, $$(PREFIX)/include/$(1))
	$(call cmd, "UNINSTALL", $$(l_so_mmb), rm -f , $$(PREFIX)/lib/$$(l_so_mmb))
	$(call cmd, "UNINSTALL", $$(l_so_m)  , rm -f , $$(PREFIX)/lib/$$(l_so_m))
	$(call cmd, "UNINSTALL", $$(l_so)    , rm -f , $$(PREFIX)/lib/$$(l_so))
	$(call cmd, "UNINSTALL", $$(l_A)     , rm -f , $$(PREFIX)/lib/$$(l_a))
	$(call cmd, "LDCONFIG" , ""         , ldconfig)

uninstall: lib$(1)_uninstall
.PHONY: lib$(1)_uninstall

lib$(1)_clean:
	$$(call clean, "lib$(1) *.o", $(5))
	$$(call clean, "lib$(1) *.d", $$(patsubst %.o,%.d,$(5)))

clean: lib$(1)_clean
.PHONY: lib$(1)_clean

lib$(1)_distclean:
	$$(call clean, $$(l_a) , lib/$$(l_a))
	$$(call clean, $$(l_so), lib/$$(l_so))

distclean: lib$(1)_distclean
.PHONY: lib$(1)_distclean


lib/$$(l_so): LDFLAGS += -Wl,-soname,$$(l_so_m)
lib/$$(l_so): LDFLAGS += -shared
lib/$$(l_so): LDFLAGS := $(filter-out -l$(1), $$(LDFLAGS))
lib/$$(l_so): $(5)
	$$(call ld, $(5), $$@)

lib/$$(l_a): $(5)
	$$(call ar, $(5), $$@)

$(5): CFLAGS += -fPIC
$(eval $(call __smk_add_compile_rule, $(5)))

-include $$(patsubst %.o,%.d,$(5))

undefine $$(l_a)
undefine $$(l_so)
undefine $$(l_so_m)
undefine $$(l_so_mm)
undefine $$(l_so_mmb)

endef

define smk_depend
bin/$(1): lib/lib$(2).so
endef
