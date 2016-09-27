# Basic build configuration
verbose		?= n
config		?= .config

ifeq (,$(filter $(MAKECMDGOALS),configure clean distclean))
-include $(config)
endif


# Applications
CHAOS_BIN	:= chaos
CHAOS_OBJ	:=
CHAOS_OBJ	+= bin/chaos.src/config.o
CHAOS_OBJ	+= bin/chaos.src/cmdline.o
CHAOS_OBJ	+= bin/chaos.src/chaos.o

# Tests

# Libraries
LIBH2_A		:= libh2.a
LIBH2_SO	:= libh2.so
LIBH2_SO_M		:= $(LIBH2_SO).$(LIBH2_V_MAJOR)
LIBH2_SO_Mm		:= $(LIBH2_SO).$(LIBH2_V_MAJOR).$(LIBH2_V_MINOR)
LIBH2_SO_MmB	:= $(LIBH2_SO).$(LIBH2_V_MAJOR).$(LIBH2_V_MINOR).$(LIBH2_V_BUGFIX)

LIBH2_OBJ	:=
LIBH2_OBJ	+= lib/h2/xen/xc.o
LIBH2_OBJ	+= lib/h2/xen/dev.o
LIBH2_OBJ	+= lib/h2/xen/vif.o
LIBH2_OBJ	+= lib/h2/xen/xs.o
LIBH2_OBJ	+= lib/h2/xen/console.o
LIBH2_OBJ	+= lib/h2/h2.o
LIBH2_OBJ	+= lib/h2/xen.o

# cscope
CSCOPE_FILES	:= cscope.files cscope.out cscope.in.out cscope.po.out

# Default build flags
CFLAGS		+= -Iinc
CFLAGS		+= -std=gnu11
CFLAGS		+= -Wall -MD -MP -g

LDFLAGS		+= -Llib

XEN_CFLAGS	:=
XEN_CFLAGS	+= -I$(XEN_ROOT)/tools/include
XEN_CFLAGS	+= -I$(XEN_ROOT)/tools/libxc/include
XEN_CFLAGS	+= -I$(XEN_ROOT)/dist/install/$(XEN_PREFIX)/include

XEN_LDFLAGS	:=
XEN_LDFLAGS	+= -L$(XEN_ROOT)/dist/install/$(XEN_PREFIX)/lib
XEN_LDFLAGS += -lxenctrl -lxenstore -lxenguest -lxentoollog

LIBH2_DEP_CFLAGS	:=
LIBH2_DEP_CFLAGS	+= $(XEN_CFLAGS)

LIBH2_DEP_LDFLAGS	:=
LIBH2_DEP_LDFLAGS	+= -lh2
LIBH2_DEP_LDFLAGS	+= -Wl,-rpath-link,$(XEN_ROOT)/dist/install/$(XEN_PREFIX)/lib


# Targets
all: libh2 chaos

tests:

install: libh2 chaos
	$(call cmd, "INSTALL", "include/h2"   , cp -r , inc/h2          $(PREFIX)/include/)
	$(call cmd, "INSTALL", $(LIBH2_SO_MmB), cp -f , lib/$(LIBH2_SO) $(PREFIX)/lib/$(LIBH2_SO_MmB))
	$(call cmd, "INSTALL", $(LIBH2_SO_M)  , ln -sf, $(LIBH2_SO_MmB) $(PREFIX)/lib/$(LIBH2_SO_M))
	$(call cmd, "INSTALL", $(LIBH2_SO)    , ln -sf, $(LIBH2_SO_MmB) $(PREFIX)/lib/$(LIBH2_SO))
	$(call cmd, "INSTALL", $(LIBH2_A)     , cp -f , lib/$(LIBH2_A)  $(PREFIX)/lib/$(LIBH2_A))
	$(call cmd, "LDCONFIG", "", ldconfig)
	$(call cmd, "INSTALL", $(CHAOS_BIN)   , cp -f , bin/$(CHAOS_BIN) $(PREFIX)/bin/$(CHAOS_BIN))

uninstall:
	$(call cmd, "UNINSTALL", "include/h2"   , rm -rf, $(PREFIX)/include/h2)
	$(call cmd, "UNINSTALL", $(LIBH2_SO_MmB), rm -f , $(PREFIX)/lib/$(LIBH2_SO_MmB))
	$(call cmd, "UNINSTALL", $(LIBH2_SO_M)  , rm -f , $(PREFIX)/lib/$(LIBH2_SO_M))
	$(call cmd, "UNINSTALL", $(LIBH2_SO)    , rm -f , $(PREFIX)/lib/$(LIBH2_SO))
	$(call cmd, "UNINSTALL", $(LIBH2_A)     , rm -f , $(PREFIX)/lib/$(LIBH2_A))
	$(call cmd, "LDCONFIG", "", ldconfig)
	$(call cmd, "UNINSTALL", $(CHAOS_BIN)   , rm -f , $(PREFIX)/bin/$(CHAOS_BIN))

configure: $(config)

cscope:
	$(call cmd, "CSCOPE", "", find . -name '*.c' -or -name '*.h' > cscope.files; cscope -b -q -k)

clean:
	$(call cmd, "CLEAN", "*.o", rm -rf, $(shell find -name "*.o"))
	$(call cmd, "CLEAN", "*.d", rm -rf, $(shell find -name "*.d"))

distclean: clean
	$(call cmd, "CLEAN", $(LIBH2_A)  , rm -f, lib/$(LIBH2_A))
	$(call cmd, "CLEAN", $(LIBH2_SO) , rm -f, lib/$(LIBH2_SO))
	$(call cmd, "CLEAN", $(CHAOS_BIN), rm -f, bin/$(CHAOS_BIN))
	$(call cmd, "CLEAN", $(config)   , rm -f, $(config))
	$(call cmd, "CLEAN", "cscope files", rm -f, $(CSCOPE_FILES))

.PHONY: all tests install uninstall configure cscope clean distclean


libh2: lib/$(LIBH2_SO) lib/$(LIBH2_A)

chaos: bin/$(CHAOS_BIN)

.PHONY: libh2 chaos


# Build rules
$(config): config.in
	$(call cmd, "CONFIG", $@, cp -n, $^ $@)

bin/$(CHAOS_BIN): LDFLAGS += -ljansson
bin/$(CHAOS_BIN): LDFLAGS += $(LIBH2_DEP_LDFLAGS)
bin/$(CHAOS_BIN): $(CHAOS_OBJ) lib/$(LIBH2_SO)
	$(call clink, $(CHAOS_OBJ), $@)

$(CHAOS_OBJ): CFLAGS += $(LIBH2_DEP_CFLAGS)

lib/$(LIBH2_SO): LDFLAGS += -shared
lib/$(LIBH2_SO): LDFLAGS += -Wl,-soname,$(LIBH2_SO_M)
# Because lib/$(LIBH2_SO) may be built as a dependency of bin/$(CHAOS_BIN) $(LDFLAGS) might contain
# $(LIBH2_DEP_LDFLAGS). We must ensure that is removed before linking lib/$(LIBH2_SO).
lib/$(LIBH2_SO): LDFLAGS += $(XEN_LDFLAGS)
lib/$(LIBH2_SO): LDFLAGS := $(filter-out $(LIBH2_DEP_LDFLAGS), $(LDFLAGS))
lib/$(LIBH2_SO): $(LIBH2_OBJ)
	$(call clink, $^, $@)

lib/$(LIBH2_A): $(LIBH2_OBJ)
	$(call cmd, "AR", $@, ar rcs, $@ $^)

$(LIBH2_OBJ): CFLAGS += -fPIC
$(LIBH2_OBJ): CFLAGS += $(XEN_CFLAGS)

%.o: %.c $(config)
	$(call ccompile, $<, $@)


# Define verbose command
ifneq ($(verbose),y)
cmd			 = @printf " %-10s %s\n" $(1) $(2) && $(3) $(4)
else
cmd			 = $(3) $(4)
endif

# Compile and link commands
ccompile	 = $(call cmd, "CC", $2, $(CC), $(CFLAGS) -c $(1) -o $(2))
clink		 = $(call cmd, "LD", $2, $(CC), $(CFLAGS) $(1) $(LDFLAGS) -o $(2))


# Include auto-generated prerequisites
-include $(CHAOS_OBJ:%.o=%.d)
-include $(LIBH2_OBJ:%.o=%.d)
